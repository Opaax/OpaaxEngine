# Opaax Engine — Audio System Design Doc

**Statut** : Design validé, pré-implémentation (M-Audio-1 non démarré)
**Auteur** : Lead Engine
**Cible** : PC (Steam / EGS), 2D platformer
**Backend recommandé** : miniaudio (à vendorer)

> Tout le pseudo-code de ce document est du **pseudo-code**, pas du C++ compilable.
> Les conventions de nommage suivent le standard moteur (PascalCase, m_Membres).

---

## 1. Contexte et objectifs

Le moteur n'a aucun système audio. Objectifs de la première itération complète :

- Layers/bus par défaut (Master → Music, SFX, Dialogue), extensibles par data.
- Séparation stricte donnée brute / définition de lecture / instance jouée.
- Play/Stop via **handles opaques** (style FMOD/Wwise), jamais de pointeurs exposés.
- Fade in/out par voix et par bus, sans clic.
- Variations par ClipDef (liste d'assets + random, pour footsteps etc.).
- Préchargement par **AudioBank**, cycle de vie lié au world ou à l'engine.
- Sons 2D et 3D (spatialisation simplifiée platformer : atténuation + pan X).

**Non-objectifs (v1)** : graphe d'effets DSP, occlusion, reverb zones, HRTF,
voix virtuelles à la FMOD, éditeur de sound cues. Le design réserve la place,
on n'implémente pas.

---

## 2. Décisions d'architecture (ADR condensé)

| # | Décision | Alternative rejetée | Raison |
|---|----------|--------------------|--------|
| D1 | Thread audio dédié + ring buffer vers le device | Mixing dans le callback device / single-thread | Min-spec PC multi-core acquis. Un seul chemin d'exécution. Débuggable. |
| D2 | Communication game→audio par **command queue SPSC lock-free** | Mutex sur l'état partagé | Zéro contention, zéro allocation, latence bornée. |
| D3 | Handles = index + génération (u64) | Pointeurs / shared_ptr sur les voix | Détection des handles périmés en O(1), no-op silencieux, pas d'ownership partagé. |
| D4 | Sélection de variation résolue **au Play(), game thread** | Résolution sur le thread audio | Thread audio bête et déterministe. RNG côté gameplay, seedable, testable. |
| D5 | Variations = liste plate + mode de sélection | Graphe de nodes à la Unreal | Un graphe = éditeur + runtime + sérialisation. Hors budget équipe. La liste couvre le besoin. |
| D6 | Préchargement = refcount de l'asset manager + AudioBank | Cache LRU dédié audio | Réutilise l'existant. Une bank = une ref forte par asset, rien de plus. |
| D7 | Deux propriétaires de banks : World subsystem + Engine (bank globale) | Tout au world | Les sons UI/menu/player survivent aux transitions de niveau. |
| D8 | Backend abstrait, miniaudio en première implémentation | WASAPI à la main / FMOD direct | miniaudio = device + décodage WAV/OGG/MP3, header-only. Migration FMOD possible sans toucher l'API publique. |

---

## 3. Vue d'ensemble

```
┌─────────────────────────────  GAME THREAD  ─────────────────────────────┐
│                                                                          │
│  World Layer (entt)                                                      │
│    AudioSourceComponent ── AudioListenerComponent                        │
│         │ (sync transform → SetPosition)                                 │
│         ▼                                                                │
│  AudioEngine (façade — seule API publique)                               │
│    Play / Stop / FadeTo / SetBusVolume / handles                         │
│    résolution des variations (D4)                                        │
│         │                                                                │
│         ▼  push commandes                                                │
│  ┌────────────────────┐                                                  │
│  │ CommandQueue (SPSC)│   + queue retour (voix terminées, events)        │
│  └────────────────────┘                                                  │
└──────────│───────────────────────────────────────────────────────────────┘
           ▼  pop commandes
┌─────────────────────────────  AUDIO THREAD  ────────────────────────────┐
│  AudioMixer                                                              │
│    Bus tree : Master → Music / SFX / Dialogue / …                        │
│  VoicePool (taille fixe, ex. 64)                                         │
│    Voice : cursor, gain, Fader, spatial state                            │
│  Mix loop : voices → gain chain → spatialize → accumulate → ring buffer  │
└──────────│───────────────────────────────────────────────────────────────┘
           ▼  ring buffer PCM
┌───────────────────────────  DEVICE CALLBACK  ───────────────────────────┐
│  Backend (miniaudio) : memcpy depuis le ring buffer. Rien d'autre.       │
└──────────────────────────────────────────────────────────────────────────┘
```

**Règle d'or** : le game thread ne touche jamais l'état audio. Tout passe par
commandes. Le callback device ne fait qu'un memcpy — le mixing est sur le
thread audio, qui remplit le ring buffer en avance (latence cible : 2 à 4
blocs de 256–512 samples).

---

## 4. Les trois concepts de données

### 4.1 AudioResource — la donnée brute

```
AudioResource:
    Format        : sampleRate, channels, frameCount
    Residency     : Decoded (PCM en mémoire)  | Streamed (fichier + décodeur)
    Data          : buffer PCM  OU  stream state
    // Immutable. Partagé. Possédé par l'AssetManager (handle + refcount).
    // Ne connaît NI volume, NI bus, NI gameplay.
```

Politique : SFX courts → `Decoded`. Musique / dialogues longs → `Streamed`
(M-Audio-5). Seuil indicatif : ~10 s ou ~2 MB décodé.

### 4.2 AudioClipDef — la définition de lecture (data designer)

```
AudioClipDef:                                  // sérialisable, édité par le design
    BusId          : hash("SFX")               // bus cible
    BaseVolume     : float [0..1]
    BasePitch      : float (1.0 = normal)
    Loop           : bool
    Spatial        : Mode2D | Mode3D
    Attenuation    : { minDist, maxDist, curve }   // ignoré en 2D

    // --- Variations (style Sound Cue plat) ---
    SelectionMode  : Single | Random | RandomNoRepeat | Sequential | Shuffle
    Entries        : list of {
                        assetRef   : AssetHandle<AudioAsset>
                        weight     : float
                        volumeMul  : float          // override optionnel
                        pitchMul   : float
                     }

    // --- Randomisation par lecture ---
    VolumeRange    : ±float                    // ex. ±0.1
    PitchRange     : ±float                    // ex. ±0.05 — casse l'effet robot
```

### 4.3 AudioInstance (Voice) — état runtime, interne au thread audio

```
Voice:
    Generation     : u32          // pour la validation de handle
    State          : Free | Playing | Paused | Stopping
    Asset          : AudioAsset*  // résolu, jamais null si non-Free
    Cursor         : frame index (ou stream state)
    Gain, Pitch    : valeurs effectives (base × overrides × random)
    BusId          : hash
    Fader          : { current, target, remainingFrames }
    Spatial        : { mode, worldPos }       // pos mise à jour par commande
    Priority       : u8                        // pour le vol de voix
```

---

## 5. Handles

```
AudioHandle = u64  →  [ index : u32 | generation : u32 ]

Validation (thread audio, à chaque commande ciblant un handle):
    voice = pool[handle.index]
    if voice.Generation != handle.generation:  → no-op silencieux
```

- Une voix recyclée incrémente sa génération → tous les anciens handles meurent.
- `INVALID_HANDLE = 0` (génération 0 jamais utilisée).
- API publique sur handle (toutes async, via commandes) :
  `Stop(handle, fadeOutMs)`, `Pause`, `Resume`, `SetVolume`, `SetPitch`,
  `SetPosition`, `FadeTo(volume, ms)`.
- `IsPlaying(handle)` : lit un **miroir game-thread** de l'état des voix,
  mis à jour par la queue retour audio→game. Jamais de lecture directe
  de l'état du thread audio.

**Voice pool** : taille fixe (64, configurable au Startup). Si plein :
vol de voix — on évince d'abord la priorité la plus basse, à priorité égale
le gain effectif le plus faible. Les voix `Stopping` sont évincées en premier.

---

## 6. Mixer et bus

```
Bus:
    Id           : hash             // "Master", "Music", "SFX", "Dialogue"
    Parent       : Bus* (null pour Master)
    Gain         : float
    Mute, Solo   : bool
    Fader        : même struct que les voix (fade de bus = gratuit)
    // TODO(v2): EffectSlot[] — réservé, non implémenté.

EffectiveGain(voice) = voice.Gain
                     × voice.Fader.current
                     × Π (bus.Gain × bus.Fader.current)  pour bus → Master
// Recalculé par bloc audio. Pas de cache, pas d'invalidation — c'est 4 mults.
```

Les trois bus par défaut sont créés au Startup. Bus additionnels déclarés
par data (fichier de config mixer). Identifiés par string hashée à la
création — **aucune comparaison de string au runtime**.

---

## 7. Fades

```
Fader.Tick(blockFrames):
    if remainingFrames == 0: current = target; return
    step = (target - current) / remainingFrames        // linéaire, par bloc
    current += step × min(blockFrames, remainingFrames)
    remainingFrames -= blockFrames
```

- **Fade-in** : `Play(clip, { fadeInMs })` → la voix démarre à gain 0,
  fader vers le gain effectif.
- **Fade-out / stop propre** : `Stop(handle, fadeOutMs)` → état `Stopping`,
  fader vers 0, la voix est libérée quand `current == 0`. C'est ce qui
  élimine les clics. `Stop(handle, 0)` = kill immédiat (avec micro-ramp de
  ~2 ms hardcodé pour éviter le clic quand même).
- **Cross-fade musique** = deux voix + deux faders. Pas de système dédié.

---

## 8. Flux d'un Play() — de bout en bout

```
GAME THREAD — AudioEngine.Play(clipDef, params):
    1. entry  = SelectEntry(clipDef)            // D4 : Random/NoRepeat/etc., RNG gameplay
    2. asset  = ResolveAsset(entry.assetRef)
         if not resident:
             LOG_WARN("Audio asset not preloaded: {}")   // règle : tout passe par une bank
             RequestAsyncLoad(entry.assetRef)            // le son partira en retard, rien ne bloque
             return INVALID_HANDLE                        // v1 : on droppe, pas de replay différé
    3. gain   = clip.BaseVolume × entry.volumeMul × Rand(±clip.VolumeRange)
       pitch  = clip.BasePitch  × entry.pitchMul  × Rand(±clip.PitchRange)
    4. handle = m_HandleAllocator.Reserve()      // réserve index+gen côté game thread
    5. push Command::Play{ handle, asset, gain, pitch, busId, loop,
                           spatial, fadeInMs, priority }
    6. return handle                              // utilisable immédiatement (async)

AUDIO THREAD — boucle:
    loop:
        DrainCommands()          // Play → alloc voice / steal ; Stop → Stopping ; Set* → apply
        while ringBuffer.FreeBlocks() > 0:
            MixBlock():
                for each voice in Playing|Stopping:
                    fader.Tick(block)
                    read frames (resample si pitch ≠ 1)
                    g = EffectiveGain(voice)
                    if 3D: g ×= Attenuate(dist); pan = PanFromRelativeX()
                    accumulate into mix buffer (stereo)
                    if voice finished or faded out: Release(voice)  → push event retour
            ringBuffer.Commit(block)
        sleep until ring buffer has room          // pacing par le buffer, pas par timer
```

---

## 9. Banks et préchargement

```
AudioBank:                        // data asset : juste une liste
    Clips : list of ClipDefRef    // précharger une bank = résoudre tous les
                                  // assets de tous les entries de tous les clips

AudioBankSubsystem (WORLD subsystem — pattern ISubsystemManager existant):
    OnWorldLoad(worldDesc):
        for bankRef in worldDesc.audioBanks:
            m_Held += AcquireStrongRefs(bank)     // decode PCM upfront (async, budgeté)
    OnWorldUnload():
        m_Held.clear()                            // refcount → les assets meurent
                                                  // sauf s'ils sont tenus ailleurs

AudioSubsystem (ENGINE — IEngineSubsystem):
    m_GlobalBank : AudioBank                      // UI, menu, player, stingers
    // Chargée au Startup, relâchée au Shutdown. Survit aux transitions. (D7)
```

**Point d'attention** : ne jamais relâcher une ref pendant qu'une voix lit
l'asset. Le thread audio tient sa propre ref sur l'asset tant que la voix
vit (prise à la commande Play, relâchée via la queue retour au Release).
L'unload d'un world ne peut donc jamais tirer le tapis sous une voix active
— au pire l'asset survit quelques centaines de ms le temps du fade-out.

---

## 10. Spatialisation 2D/3D (world layer)

```
AudioListenerComponent : { }                      // tag, transform = celui de l'entité (caméra)
AudioSourceComponent   : { clipRef, handle, autoPlay, // glue pure, zéro logique
                           stopOnDestroy : bool }

AudioSpatialSystem.Update():                      // système entt, game thread
    listenerPos = transform of listener entity
    push Command::SetListener{ listenerPos }
    for each (AudioSourceComponent, Transform) with valid handle:
        push Command::SetPosition{ handle, transform.pos }
        // PERF: dirty-check la position, ne push que si elle a bougé.

Thread audio — 3D:
    dist = |voice.pos - listener.pos|
    g   ×= EvaluateCurve(clip.Attenuation, dist)   // Linear | InverseSquare | Custom
    pan  = clamp((voice.pos.x - listener.pos.x) / clip.Attenuation.maxDist, -1, 1)
    // Platformer 2D : pan sur l'axe X suffit. Pas de HRTF, pas de doppler (v1).

2D : gain + pan constant (param du Play). Point final.
```

`OnDestroy` du component : si `stopOnDestroy` → `Stop(handle, shortFade)`.
Sinon la voix vit sa vie (fire-and-forget, ex. son d'explosion après la
mort de l'entité).

---

## 11. Backend

```
IAudioBackend:
    Init(sampleRate, channels, blockFrames, ringBuffer*)  → bool
    Shutdown()
    // Le callback device fourni par le backend fait UNIQUEMENT :
    //   read ringBuffer → output ; si underrun → silence + compteur (jamais de blocage)

MiniaudioBackend : IAudioBackend    // première et seule implémentation v1
    // miniaudio fournit aussi le décodage WAV/OGG/MP3 → utilisé par l'AssetManager
    // pour AudioAsset (pas seulement le device).
```

L'abstraction existe pour une éventuelle migration FMOD sans toucher
`AudioEngine`. On ne construit **pas** de deuxième backend en v1.

---

## 12. Threading — contrats

| Frontière | Mécanisme | Contrat |
|---|---|---|
| Game → Audio | CommandQueue SPSC lock-free, capacité fixe (ex. 1024) | Jamais d'allocation. Si pleine : drop + `LOG_ERROR` (ne doit jamais arriver — dimensionner large). |
| Audio → Game | EventQueue SPSC (VoiceFinished, AssetReleased, Underrun) | Drainée par `AudioEngine.Update()` chaque frame → met à jour le miroir d'état + relâche les refs d'assets. |
| Audio → Device | Ring buffer PCM | Le callback ne bloque jamais. Underrun = silence + métrique. |
| Handles | Réservation côté game thread (allocateur d'index/gen dédié) | Le thread audio ne crée jamais de handle ; il occupe l'index réservé. |

Aucun mutex nulle part sur le chemin chaud. Le seul état partagé est dans
les queues et le ring buffer.

---

## 13. Découpage en milestones

| Milestone | Contenu | Done quand… |
|---|---|---|
| **M-Audio-1** | Vendor miniaudio, `IAudioBackend`, thread audio, ring buffer, CommandQueue/EventQueue, VoicePool, handles, Play/Stop (WAV decoded), bus Master seul | Un WAV joue et s'arrête proprement via handle ; kill de 64 voix sans clic ni crash ; handle périmé = no-op vérifié par test. |
| **M-Audio-2** | Bus tree (Music/SFX/Dialogue + data-driven), Faders voix + bus, `Stop(fade)`, `FadeTo` | Fade-out de bus Music pendant que SFX continue ; zéro clic à l'oscillo. |
| **M-Audio-3** | `AudioClipDef` sérialisé, variations + modes de sélection, randomisation vol/pitch, intégration AssetManager | 5 footsteps en RandomNoRepeat, jamais deux identiques consécutifs (test seedé). |
| **M-Audio-4** | `AudioBank`, `AudioBankSubsystem` (world), bank globale engine, warning asset non résident, refs tenues par le thread audio | Load/unload de world en boucle : zéro reload d'assets de la bank globale, zéro use-after-free (ASan). |
| **M-Audio-5** | Composants entt + `AudioSpatialSystem`, atténuation + pan X, streaming OGG pour la musique | Musique streamée 3 min + 30 SFX 3D simultanés, thread audio < 1 ms/bloc en Release. |

---

## 14. Risques identifiés

- **FIXME (design)** : le drop silencieux d'un `Play()` sur resource non résident
  (§8, étape 2) est acceptable en v1 mais frustrant en prod — envisager un
  replay différé (M-Audio-4+).
- **Pitch ≠ 1.0** implique du resampling par voix → c'est le coût CPU dominant.
  Resampler linéaire en v1 (qualité suffisante pour ±5 % de pitch random) ;
  ne pas partir sur du sinc sans profiling.
- **Capacité des queues** : dimensionner par le pire cas mesuré (spam de
  footsteps + burst de particules sonores), pas au doigt mouillé.
- Le miroir d'état game-thread (`IsPlaying`) a **une frame de latence** par
  construction. C'est documenté, c'est voulu, le gameplay doit vivre avec. (better solution?)

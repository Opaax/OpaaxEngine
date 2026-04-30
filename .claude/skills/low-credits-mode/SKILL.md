\---

name: low-credit-mode

description: Choose a task that can be done with the remaining credits of the account.

disable-model-invocation: true

\---



\## Instructions



\### Step 0 — Activation Validation (IMPORTANT)

Before doing anything:

\- Confirm at least one condition:

&#x20; - User explicitly mentions low credits / budget constraints

&#x20; - System strongly suspects limited budget

\- If NOT confirmed → EXIT skill immediately



\### Step 1 — Budget Estimation

Estimate remaining credits or reasoning allowance:

\- VERY\_LOW: critical constraint

\- LOW: constrained

\- UNKNOWN: assume LOW (fail-safe)



\### Step 2 — Task Cost Evaluation

Classify request cost:

\- CHEAP: trivial modification or lookup

\- MODERATE: small system change

\- EXPENSIVE: architecture, multi-system, exploration



\### Step 3 — Hard Constraints



\#### If VERY\_LOW:

\- Only allow CHEAP tasks

\- If task is MODERATE or EXPENSIVE:

&#x20; → Return reduced scope OR refuse with minimal guidance



\#### If LOW:

\- Allow CHEAP and MODERATE

\- EXPENSIVE → reduce scope automatically





\### Step 4 — Minimal Context Loading

\- Read CLAUDE.Dev.md

\- Extract ONLY directly relevant sections

\- Do NOT summarize entire file

\- Do NOT explore unrelated systems





\### Step 5 — Execution Strategy

\- No exploration

\- No multiple solutions

\- No long explanations

\- Prefer:

&#x20; - direct code

&#x20; - direct answer

&#x20; - existing engine patterns





\### Step 6 — Output Rules

Output MUST be:

\- Short

\- Actionable

\- Single solution



Append:

"Mode: LOW\_CREDIT"

"Scope: REDUCED | FULL"


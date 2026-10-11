"""Turns the TestWorld feature tests' failures into GitHub annotations.

Usage: python report_feature_failures.py <build folder> <TestWorld folder>

Each game script (TestWorld/Tests/<Name>.json) writes its answers to
<build>/TestWorldResults/<Name>.out.json, each editor script (TestWorld/EditorTests/<Name>.json) to
Editor<Name>.out.json. A failed request is reported with its id, command and error; a script with no
answers, or one that never finished, is reported too (the app crashed or hung). A test CTest failed
while every request passed is reported as a failure of the app itself (a crash on exit, a timeout).
Annotations are limited per step, so at most 10 are errors and the rest notices.
"""
import json
import sys
from pathlib import Path

# (scripts folder, answers file prefix, CTest name prefix)
SUITES = (("Tests", "", "TestWorld."), ("EditorTests", "Editor", "TestWorldEditor."))


def failed_in_ctest(build):
    """The test names CTest reported failed ("12:TestWorld.UI" lines), and when it wrote them."""
    log = build / "Testing" / "Temporary" / "LastTestsFailed.log"
    if not log.exists():
        return set(), 0.0
    names = {line.split(":", 1)[-1].strip() for line in log.read_text(encoding="utf-8", errors="replace").splitlines()}
    return names, log.stat().st_mtime


def failures(build, testworld):
    """(test, message) for every failure, in suite then name order."""
    results = build / "TestWorldResults"
    failed, failed_at = failed_in_ctest(build)
    found = []
    for folder, prefix, ctest_prefix in SUITES:
        for script in sorted((testworld / folder).glob("*.json")):
            test = ctest_prefix + script.stem
            answers = results / f"{prefix}{script.stem}.out.json"
            if not answers.exists():
                found.append((test, "no answers written: the app did not start or crashed at once"))
                continue

            try:
                data = json.loads(answers.read_text(encoding="utf-8"))
            except (OSError, ValueError) as error:
                found.append((test, f"unreadable answers: {error}"))
                continue

            reported = len(found)
            for response in data.get("responses", []):
                if not response.get("ok", False):
                    found.append((test, f"'{response.get('id')}' ({response.get('command')}): {response.get('error')}"))

            if not data.get("done", False):
                answered = len(data.get("responses", []))
                found.append((test, f"the script did not finish ({answered} request(s) answered): the app "
                                    "crashed, hung or was stopped"))

            # CTest keeps the list when a later run passes: a test answered after it was written passed.
            if len(found) == reported and test in failed and answers.stat().st_mtime <= failed_at:
                found.append((test, "every request passed, but the run failed: the app crashed or hung while "
                                    "closing, or ran out of time"))
    return found


def main():
    build, testworld = Path(sys.argv[1]), Path(sys.argv[2])
    found = failures(build, testworld)
    print(f"{len(found)} feature test failure(s)")

    for index, (test, message) in enumerate(found[:30]):
        level = "error" if index < 10 else "notice"
        message = message.replace("%", "%25").replace("\r", "").replace("\n", " ")
        print(f"::{level} title={test}::{message[:500]}")


if __name__ == "__main__":
    main()

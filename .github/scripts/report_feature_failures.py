"""Turns the TestWorld feature tests' failures into GitHub annotations.

Usage: python report_feature_failures.py <build folder> <TestWorld/Tests folder>

Each test writes its answers to <build>/TestWorldResults/<Feature>.out.json. A failed request is
reported with its id, command and error; a script with no answers, or one that never finished, is
reported too (the game crashed or hung). A test CTest failed while every request passed is reported
as a failure of the game itself (a crash on exit, a timeout). Annotations are limited per step, so
at most 10 are errors and the rest notices.
"""
import json
import sys
from pathlib import Path


def failed_in_ctest(build):
    """The feature names CTest reported failed ("12:TestWorld.UI" lines), and when it wrote them."""
    log = build / "Testing" / "Temporary" / "LastTestsFailed.log"
    if not log.exists():
        return set(), 0.0
    names = set()
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        test = line.split(":", 1)[-1].strip()
        if test.startswith("TestWorld."):
            names.add(test[len("TestWorld."):])
    return names, log.stat().st_mtime


def failures(build, tests):
    """(feature, message) for every failure, in feature order."""
    results = build / "TestWorldResults"
    failed, failed_at = failed_in_ctest(build)
    found = []
    for script in sorted(tests.glob("*.json")):
        feature = script.stem
        answers = results / f"{feature}.out.json"
        if not answers.exists():
            found.append((feature, "no answers written: the game did not start or crashed at once"))
            continue

        try:
            data = json.loads(answers.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            found.append((feature, f"unreadable answers: {error}"))
            continue

        reported = len(found)
        for response in data.get("responses", []):
            if not response.get("ok", False):
                found.append((feature, f"'{response.get('id')}' ({response.get('command')}): {response.get('error')}"))

        if not data.get("done", False):
            answered = len(data.get("responses", []))
            found.append((feature, f"the script did not finish ({answered} request(s) answered): the game "
                                   "crashed, hung or was stopped"))

        # CTest keeps the list when a later run passes: a test answered after it was written passed.
        if len(found) == reported and feature in failed and answers.stat().st_mtime <= failed_at:
            found.append((feature, "every request passed, but the run failed: the game crashed or hung while "
                                   "closing, or ran out of time"))
    return found


def main():
    build, tests = Path(sys.argv[1]), Path(sys.argv[2])
    found = failures(build, tests)
    print(f"{len(found)} feature test failure(s)")

    for index, (feature, message) in enumerate(found[:30]):
        level = "error" if index < 10 else "notice"
        message = message.replace("%", "%25").replace("\r", "").replace("\n", " ")
        print(f"::{level} title=TestWorld.{feature}::{message[:500]}")


if __name__ == "__main__":
    main()

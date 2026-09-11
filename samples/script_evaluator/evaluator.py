import argparse


def parse_cases(path):
    cases = []
    current = None
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("KADOKA_SCRIPT_EVALUATOR"):
                continue
            parts = line.split()
            if parts[0] == "case":
                current = {"id": int(parts[1]), "features": {}}
            elif parts[0] == "feature" and current is not None:
                current["features"][parts[1]] = float(parts[2])
            elif parts[0] == "end" and current is not None:
                cases.append(current)
                current = None
    return cases


def evaluate(features):
    # Reference implementation only. Real models can use arbitrary logic here.
    score = 0.0
    for key, value in features.items():
        if key.endswith("bonus"):
            score += value
        elif key.endswith("penalty"):
            score -= value
        else:
            score += value * 0.1
    return {"score": score, "confidence": min(1.0, abs(score) / 10.0)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--kadoka-eval-input", required=True)
    parser.add_argument("--kadoka-eval-output", required=True)
    args = parser.parse_args()

    cases = parse_cases(args.kadoka_eval_input)
    with open(args.kadoka_eval_output, "w", encoding="utf-8") as f:
        for case in cases:
            result = evaluate(case["features"])
            f.write(f"result {case['id']}\n")
            for key, value in result.items():
                f.write(f"value {key} {value}\n")
            f.write("diag runtime=python_process\n")
            f.write("end\n")


if __name__ == "__main__":
    main()

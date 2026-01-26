import json
import argparse

def get_value_by_path(data, path):
    current = data
    for part in path.split():
        if isinstance(current, list):
            part = int(part)
        current = current[part]
    return current

def set_value_by_path(data, path, value):
    parts = path.split()
    current = data
    for part in parts[:-1]:
        if isinstance(current, list):
            part = int(part)
        current = current[part]
    last_part = parts[-1]
    if isinstance(current, list):
        last_part = int(last_part)
    current[last_part] = value

def extract(args):
    with open(args.input, "r", encoding="utf-8") as f:
        data = json.load(f)

    value = get_value_by_path(data, args.path)

    if not isinstance(value, str):
        raise ValueError("The value at the specified path is not a JSON string")

    inner_json = json.loads(value, object_pairs_hook=dict)

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(inner_json, f, indent=args.indent, sort_keys=False)

    print(f"✔ Inner JSON extracted to {args.output}")

def incorporate(args):
    with open(args.input, "r", encoding="utf-8") as f:
        outer_json = json.load(f)

    with open(args.inner, "r", encoding="utf-8") as f:
        inner_json = json.load(f)

    inner_json_str = json.dumps(inner_json, separators=(",", ":"), ensure_ascii=False)
    set_value_by_path(outer_json, args.path, inner_json_str)

    output_file = args.input if args.inplace else args.output

    if not args.inplace and not output_file:
        raise ValueError("Output file must be specified if not using --inplace")

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(outer_json, f, indent=args.indent, sort_keys=False)

    action = "updated in-place" if args.inplace else f"written to {args.output}"
    print(f"✔ Inner JSON incorporated and {action}")

def main():
    parser = argparse.ArgumentParser(description="Extract or incorporate inner JSON")
    subparsers = parser.add_subparsers(dest="command", required=True)

    parser_extract = subparsers.add_parser("extract", help="Extract inner JSON from a JSON file")
    parser_extract.add_argument("-i", "--input", required=True, help="Input JSON file")
    parser_extract.add_argument("-o", "--output", required=True, help="Output JSON file")
    parser_extract.add_argument("-p", "--path", required=True, help='Space-separated path to inner JSON (e.g. "workflow nodes 10 config")')
    parser_extract.add_argument("--indent", type=int, default=2, help="Indentation level for output JSON (default: 2)")
    parser_extract.set_defaults(func=extract)

    parser_incorp = subparsers.add_parser("incorporate", help="Insert inner JSON into a JSON file")
    parser_incorp.add_argument("-i", "--input", required=True, help="Outer JSON file")
    parser_incorp.add_argument("-o", "--output", help="Output JSON file (ignored if --inplace)")
    parser_incorp.add_argument("-p", "--path", required=True, help='Space-separated path to insert inner JSON string')
    parser_incorp.add_argument("--inner", required=True, help="Inner JSON file to insert")
    parser_incorp.add_argument("--indent", type=int, default=2, help="Indentation level for output JSON (default: 2)")
    parser_incorp.add_argument("--inplace", action="store_true", help="Update the outer JSON file in-place instead of writing to a new file")
    parser_incorp.set_defaults(func=incorporate)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()


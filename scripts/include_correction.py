import os
import re
import argparse
from pathlib import Path


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("project_root", type=str)
    parser.add_argument("--internal", nargs="+", required=True)
    parser.add_argument("--external", nargs="*", default=[])
    parser.add_argument(
        "--extensions", nargs="+", default=[".c", ".cpp", ".h", ".hpp", ".cc", ".cxx"]
    )
    parser.add_argument(
        "--exclude", nargs="*", default=["build", "third_party", "extern"]
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--no-fullpath", action="store_true")
    parser.add_argument("--silent", action="store_true")
    return parser.parse_args()


def find_header_file(header_path, current_dir, internal_dirs):
    current_full = os.path.join(current_dir, header_path)
    if os.path.isfile(current_full):
        return os.path.normpath(current_full)

    for dir_path in internal_dirs:
        full_path = os.path.join(dir_path, header_path)
        if os.path.isfile(full_path):
            return os.path.normpath(full_path)

    return None


def is_internal_header(header_path, internal_dirs, file_path, project_root):
    if header_path.startswith("/"):
        return False

    current_dir = os.path.dirname(file_path)
    abs_header_path = find_header_file(header_path, current_dir, internal_dirs)

    if abs_header_path:
        try:
            common_path = os.path.commonpath([project_root, abs_header_path])
            return common_path == project_root
        except ValueError:
            return False

    return False


def get_complete_header_path(header_path, file_path, internal_dirs, project_root):
    current_dir = os.path.dirname(file_path)
    abs_header_path = find_header_file(header_path, current_dir, internal_dirs)

    if abs_header_path:
        best_rel_path = None
        min_length = float("inf")

        for base_dir in internal_dirs:
            if abs_header_path.startswith(base_dir):
                rel_path = os.path.relpath(abs_header_path, base_dir)
                if len(rel_path) < min_length:
                    best_base = base_dir
                    best_rel_path = rel_path
                    min_length = len(rel_path)

        if best_rel_path:
            return best_rel_path.replace("\\", "/")

        try:
            rel_path = os.path.relpath(abs_header_path, project_root)
            return rel_path.replace("\\", "/")
        except ValueError:
            pass

    return header_path


def process_file(
    file_path,
    project_root,
    internal_dirs,
    dry_run=False,
    verbose=False,
    no_fullpath=False,
):
    try:
        with open(file_path, "r", encoding="utf-8", newline="") as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        if verbose:
            print(f"   ⚠️ Skip file: {file_path}")
        return False
    except Exception as e:
        if verbose:
            print(f"  ❌ Read file error: {file_path} - {str(e)}")
        return False

    include_pattern = re.compile(r'^(\s*#include\s*)([<"])([^>"]+)([>"])(.*)$')

    modified = False
    changes = []
    new_lines = []

    for line_number, original_line in enumerate(lines, 1):
        match = include_pattern.match(original_line)
        if not match:
            new_lines.append(original_line)
            continue

        prefix = match.group(1)
        start_quote = match.group(2)
        header_path = match.group(3)
        end_quote = match.group(4)
        suffix = match.group(5)

        internal = is_internal_header(
            header_path, internal_dirs, file_path, project_root
        )

        new_header_path = header_path
        if internal and not no_fullpath:
            new_header_path = get_complete_header_path(
                header_path, file_path, internal_dirs, project_root
            )

        if internal:
            correct_quotes = ('"', '"')
        else:
            correct_quotes = ("<", ">")

        path_changed = header_path != new_header_path
        quote_changed = (
            start_quote != correct_quotes[0] or end_quote != correct_quotes[1]
        )
        need_modify = path_changed or quote_changed

        if not need_modify:
            new_lines.append(original_line)
            continue

        new_line = (
            f"{prefix}{correct_quotes[0]}{new_header_path}{correct_quotes[1]}{suffix}"
        )

        changes.append(
            {
                "line": line_number,
                "original": original_line.rstrip("\n"),
                "new": new_line.rstrip("\n"),
                "internal": internal,
                "path_changed": path_changed,
                "quote_changed": quote_changed,
            }
        )

        new_lines.append(new_line)
        modified = True

    if modified and not dry_run:
        try:
            with open(file_path, "w", encoding="utf-8", newline="") as f:
                f.writelines(new_lines)
        except Exception as e:
            if verbose:
                print(f"   ❌ Write file error: {file_path} - {str(e)}")
            return False

    if verbose and changes:
        print(f"📄 File: {file_path}")
        for change in changes:
            print(f"   📍 Line {change['line']}:")
            print(f"     Origin: {change['original']}")
            print(f"     New: {change['new']}")
            change_details = []
            if change["path_changed"]:
                change_details.append("Path completeness")
            if change["quote_changed"]:
                change_details.append(
                    f"Fixed ({change['internal'] and 'internal' or 'outside'})"
                )
            if change_details:
                print(f"     ({', '.join(change_details)})")

    return modified


def main():
    args = parse_arguments()

    project_root = Path(args.project_root).resolve()
    if not project_root.exists() or not project_root.is_dir():
        print(f"❌ Error: project path not exists '{project_root}'")
        return

    project_root_str = str(project_root)
    internal_dirs = [str(project_root / Path(d)) for d in args.internal]

    for d in internal_dirs:
        if not os.path.exists(d):
            print(f"⚠️ Warning: internal path not exists '{d}'")

    files_to_process = []
    exclude_dirs = [str(project_root / d) for d in args.exclude]

    for ext in args.extensions:
        for file_path in project_root.rglob(f"*{ext}"):
            if any(str(file_path).startswith(ex_dir) for ex_dir in exclude_dirs):
                continue
            files_to_process.append(file_path)

    verbose = args.verbose
    if verbose:
        print(f"🔍 {len(files_to_process)} files found to be processed")
        print(f"📂 Internal dirctories: {[str(d) for d in internal_dirs]}")
        print(f"🚫 Exclude directories: {exclude_dirs}")
        print(f"ℹ️  File extensions: {args.extensions}")
        if args.dry_run:
            print("🚧 Pilot mode - no actual modification of the file is made")
        if args.no_fullpath:
            print("🚫 Full path conversion is disabled")
        print("=" * 60)

    modified_count = 0
    for idx, file_path in enumerate(files_to_process, 1):
        if verbose:
            print(f"🔧 Processing ({idx}/{len(files_to_process)}): {file_path}")

        modified = process_file(
            file_path,
            project_root_str,
            internal_dirs,
            args.dry_run,
            args.verbose,
            args.no_fullpath,
        )

        if modified:
            modified_count += 1

    if not args.silent:
        print("\n" + "=" * 60)
        print(f"✅ Processing finished!")
        print(f"  Scanned files count: {len(files_to_process)}")
        print(f"  Modified files count: {modified_count}")
        if args.dry_run:
            print(f"  Mode: Test run (no actual modification of the file)")
        else:
            print(f"  Mode: Actual modification")
        if args.no_fullpath:
            print(f"  Full path conversion: Disabled")
        else:
            print(f"  Full path conversion: Enabled")
        print("=" * 60)


if __name__ == "__main__":
    main()

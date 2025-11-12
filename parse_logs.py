#!/usr/bin/env python3
"""
Parse HVAC log files and output TSV files grouped by device name.
"""

import sys
import gzip
import re
from collections import defaultdict
from datetime import datetime
from pathlib import Path


def parse_line(line):
    """
    Parse a log line and extract timestamp, name, label, and key-value pairs.
    Returns: (timestamp, name, label, kv_dict) or None if not a valid line
    """
    # Match: TIMESTAMP NAME CONTEXT LABEL: key=value ...
    # Example: 2025-11-12T05:26:25.361003+00:00 hvac_ctrl_office CTRL: FreshAir: t=23.4 ...
    match = re.match(r'^(\S+)\s+(\S+)\s+\S+\s+(\w+):\s+(.+)$', line)
    if not match:
        return None

    timestamp_str, name, label, rest = match.groups()

    # Parse key-value pairs
    kv_dict = {}
    for kv in rest.split():
        if '=' in kv:
            key, value = kv.split('=', 1)
            kv_dict[key] = value

    # Parse timestamp
    try:
        # Handle ISO format with timezone
        timestamp = datetime.fromisoformat(timestamp_str)
    except ValueError:
        return None

    return timestamp, name, label, kv_dict


def open_file(filepath):
    """Open a file, handling gzip compression and stdin."""
    if filepath == '-':
        return sys.stdin

    path = Path(filepath)
    if path.suffix == '.gz':
        return gzip.open(filepath, 'rt', encoding='utf-8')
    else:
        return open(filepath, 'r', encoding='utf-8')


def process_files(filepaths):
    """
    Process log files and extract paired FreshAir/ctrl lines.
    Returns: dict mapping name -> list of data rows
    """
    data_by_name = defaultdict(list)

    for filepath in filepaths:
        with open_file(filepath) as f:
            lines = f.readlines()

        i = 0
        while i < len(lines):
            line = lines[i].strip()

            # Try to parse current line
            parsed = parse_line(line)
            if not parsed:
                i += 1
                continue

            timestamp, name, label, kv_dict = parsed

            # Check if this is a FreshAir line
            if label == 'FreshAir':
                # Check if next line is a ctrl line with the same name
                if i + 1 < len(lines):
                    next_line = lines[i + 1].strip()
                    next_parsed = parse_line(next_line)

                    if next_parsed:
                        next_timestamp, next_name, next_label, next_kv_dict = next_parsed

                        if next_name == name and next_label == 'ctrl':
                            # We have a valid pair!
                            row = {'timestamp': timestamp}

                            # Add FreshAir data with prefix
                            for key, value in kv_dict.items():
                                row[f'freshair_{key}'] = value

                            # Add ctrl data with prefix
                            for key, value in next_kv_dict.items():
                                row[f'ctrl_{key}'] = value

                            data_by_name[name].append(row)

                            # Skip the next line since we've processed it
                            i += 2
                            continue

            i += 1

    return data_by_name


def write_tsv(name, rows):
    """Write data rows to a TSV file."""
    if not rows:
        return

    # Collect all unique keys across all rows
    all_keys = set()
    for row in rows:
        all_keys.update(row.keys())

    # Sort keys: timestamp first, then alphabetically
    all_keys.discard('timestamp')
    sorted_keys = ['timestamp'] + sorted(all_keys)

    # Write TSV
    output_file = f"{name}.tsv"
    with open(output_file, 'w', encoding='utf-8') as f:
        # Write header
        f.write('\t'.join(sorted_keys) + '\n')

        # Write data rows
        for row in rows:
            values = []
            for key in sorted_keys:
                if key == 'timestamp':
                    # Format timestamp for Excel/Sheets (without microseconds and timezone)
                    ts = row.get(key, '')
                    if isinstance(ts, datetime):
                        # Format as: YYYY-MM-DD HH:MM:SS
                        ts = ts.strftime('%Y-%m-%d %H:%M:%S')
                    values.append(str(ts))
                else:
                    values.append(str(row.get(key, '')))
            f.write('\t'.join(values) + '\n')

    print(f"Wrote {len(rows)} rows to {output_file}")


def main():
    if len(sys.argv) < 2:
        print("Usage: parse_logs.py <file1> [file2] ... [or '-' for stdin]")
        sys.exit(1)

    filepaths = sys.argv[1:]

    # Process all files
    data_by_name = process_files(filepaths)

    # Write TSV files for each name
    for name, rows in data_by_name.items():
        write_tsv(name, rows)


if __name__ == '__main__':
    main()

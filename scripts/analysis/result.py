import os
import re

def parse_log_file(log_file_path):
    # Extract model and precision from the log file name
    file_name = os.path.basename(log_file_path)
    match = re.match(r'(\d+)_precision_(\d+)', file_name)
    model = match.group(1) if match else "Unknown"
    precision = f"{match.group(2)}" if match else "None"

    # Initialize variables to store extracted data
    final_add_built = False
    adds_added = 0
    time_taken = None

    with open(log_file_path, 'r') as log_file:
        lines = log_file.readlines()

    # Parse the log file line by line
    for line in lines:
        # Count the number of "ADD added" occurrences
        if "ADD" in line and "added" in line:
            adds_added += 1

        # Check if "Final ADD built" is present
        if "Final ADD built" in line:
            final_add_built = True

        # Extract time taken from the last line
        if "gtimeout" in line:
            match = re.search(r'([\d.]+)s total', line)
            if match:
                time_taken = match.group(1)

    return model, precision, final_add_built, adds_added, time_taken


def process_all_logs(logs_folder, output_file_path):
    # Prepare the header for the tabular file
    results = [["Model", "Precision", "Final ADD Built", "Number of ADDs Added", "Time Taken (s)"]]

    # Iterate through all .log files in the folder
    for file_name in os.listdir(logs_folder):
        if file_name.endswith(".log"):
            log_file_path = os.path.join(logs_folder, file_name)
            model, precision, final_add_built, adds_added, time_taken = parse_log_file(log_file_path)

            # Append the extracted data to the results list
            results.append([
                model,
                precision,
                "Yes" if final_add_built else "No",
                adds_added,
                time_taken if time_taken else "None"
            ])

    # Write the results to the output file in tabular format
    with open(output_file_path, 'w') as output_file:
        for row in results:
            output_file.write("\t".join(map(str, row)) + "\n")


# File paths
logs_folder = "/Users/chaitanya/Desktop/sem6/rnd/xgboost/counting/results"
output_file_path = "/Users/chaitanya/Desktop/sem6/rnd/xgboost/counting/results/results_summary.txt"

# Process all log files and generate the summary
process_all_logs(logs_folder, output_file_path)

print(f"Results summary has been written to {output_file_path}")
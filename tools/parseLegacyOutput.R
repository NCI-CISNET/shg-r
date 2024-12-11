library(XML)

# Function to parse the text file and extract data between <RUN> and </RUN> tags
parse_run_data <- function(file_path, column_names) {
  # Read the entire file into a character vector
  file_content <- readLines(file_path)

# Filter lines from 136 to 100135
file_content <- file_content[136:100135]
# doc <- htmlTreeParse(file_content, useInternalNodes = TRUE)
# run_data <- xpathApply(doc, "//run", xmlValue)[[1]]
# Split the lines into separate elements in a character vector
# //run_data <- strsplit(run_data, "\n", fixed = TRUE)[[1]]

# Count the number of rows
num_rows <- length(file_content)
run_data <- file_content

# Print the number of rows
print(num_rows)

# Remove empty rows
#run_data <- run_data[run_data != ""]

# Split each row into columns based on the semicolon delimiter
run_data <- strsplit(run_data, ";")
max_cols <- sapply(run_data, function(x) length(x))
max_num_cols <- max(max_cols)
print(max_num_cols)

run_data <- lapply(run_data, function(x) {
    if (length(x) >= 7) {
        number_cols <- length(x)
        # Merge columns 7 through max_cols into a single string
        merged_values <- paste0(x[7:number_cols], collapse = " ")

        #add parantheses around each double value
        merged_values <- gsub("([0-9]+\\.[0-9]+)", "(\\1)", merged_values)

        # Convert double values to integers
        merged_values <- gsub("\\.\\d+", "", merged_values)

        # Add , after each ) including the last one
        merged_values <- gsub("\\) ", "), ", merged_values)
        merged_values <- paste0(merged_values, ", ")

        # Return the modified row
    } else {
        merged_values <- ""
    }
    c(x[1:6], merged_values)

})

run_data <- lapply(run_data, function(x) x[1:7])

# Convert the list of rows into a data frame
run_df <- do.call(rbind, lapply(run_data, function(x) {
  # Check if the number of elements in x matches the number of columns
  if (length(x) == length(column_names)) {
    data.frame(t(x), stringsAsFactors = FALSE)
  } else {
    # If the number of elements does not match, create a data frame with NA values
    data.frame(matrix(NA, nrow = 1, ncol = length(column_names)))
  }
}))
# Convert values in columns 1-6 to integers
run_df[, 1:6] <- lapply(run_df[, 1:6], as.integer)

# Set the column names to match MT_SIM
colnames(run_df) <- column_names
return(run_df)

}

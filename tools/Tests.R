# Assuming RNGSTREAM_SIM and MT_SIM are matrices or data frames

#Eventually, one of the tests to run regularily is the CMD check for R packages.
#library(rcmdcheck)
#rcmdcheck(args = "--no-manual")

# Test if the dimensions of RNGSTREAM_SIM and MT_SIM are the same
test1 <- identical(dim(RNGSTREAM_SIM), dim(MT_SIM))

# Test if all values in RNGSTREAM_SIM and MT_SIM are the same
test2 <- identical(RNGSTREAM_SIM, MT_SIM)

# Combine the test results
test_result <- test1 && test2

# Print the test result
print(test_result)

# Compute the number of duplicate rows in MT_SIM dataframe
# Print the number of duplicate rows
#print(paste0("Percentage of duplicate rows: ", round(sum(duplicated(MT_SIM))/N * 100, 2), "%"))1

# Test if any rows are completely blank in RNGSTREAM_SIM
#test3 <- any(apply(RNGSTREAM_SIM, 1, function(row) all(is.na(row) | row == "")))
# View duplicate rows in RNGSTREAM
# Filter RNGSTREAM_SIM to include one row for each duplicated row and the count of duplications as a new column
# Find duplicate rows in RNGSTREAM_SIM


# duplicated_rows <- RNGSTREAM_SIM[duplicated(RNGSTREAM_SIM), ]

# # Count the number of times each duplicate row appears
# duplicate_counts <- table(duplicated_rows)

# # Filter out rows with no duplicates
# duplicate_counts <- duplicate_counts[duplicate_counts > 1]

# # Sort by number of duplicates in descending order
# duplicate_counts <- sort(duplicate_counts, decreasing = TRUE)

# # Print the unique rows and their duplicate counts
# print(duplicate_counts)

# # Print the filtered rows
# print(duplicated_rows)

# # Test if any rows are completely blank in ordered_MT_SIM
# test3 <- any(apply(ordered_RNGSTREAM_SIM, 1, function(row) all(is.na(row) | row == "")))

# # Test if any rows are completely blank in MT_SIM
# test4 <- any(apply(MT_SIM, 1, function(row) all(is.na(row) | row == "")))

# Combine the test results
test_result <- test1 && test2

# Print the test result
print(test_result)

#print(paste0("Percentage of duplicate rows: ", round(sum(duplicated(MT_SIM))/N * 100, 2), "%"))


# # View the first 10 records
# head(RNGSTREAM_SIM, 10)

# # View the 10000th record + 9 next records
# RNGSTREAM_SIM[10000:(10000+9), ]
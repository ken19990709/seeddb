# SeedDB source library
add_subdirectory(common)
add_subdirectory(parser)
add_subdirectory(storage)
# add_subdirectory(executor)  # Disabled: circular link deps with buffer pool
# add_subdirectory(cli)       # Disabled: depends on executor + storage with buffer pool

    cmake_minimum_required(VERSION 3.15.0)

    include(FetchContent)

    FetchContent_Declare(
        aligned-allocator
        GIT_REPOSITORY ${HA_GIT_REPOSITORY_URL}/hansen-audio/aligned-allocator.git
        GIT_TAG origin/master
    )

    FetchContent_MakeAvailable(aligned-allocator)

    cmake_minimum_required(VERSION 3.15.0)

    include(FetchContent)

    FetchContent_Declare(
        dsp-tool-box
        GIT_REPOSITORY ${HA_GIT_REPOSITORY_URL}/hansen-audio/dsp-tool-box.git
        GIT_TAG origin/master
    )

    FetchContent_MakeAvailable(dsp-tool-box)

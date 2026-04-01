set(sources
    src/HV5622.c
    src/NixieDisplay.c
    src/ModeSwitch.c
    src/SmartClock.c
    src/Connectivity.c
    src/NixieHttpServer.c
    src/Storage.c
    )

set(include_dirs "includes/")


idf_component_register(SRCS "main.c" ${sources}
                    INCLUDE_DIRS ${include_dirs})

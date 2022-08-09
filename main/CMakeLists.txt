set(srcs 
    "example_ble_sec_gatts_demo.c"
    "test.c"
)

set(binary_files 
     "../wifi.cfg"
)


idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "."
    EMBED_FILES "${binary_files}"
)
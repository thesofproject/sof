# Loadable Modules Dev Kit

***TODO: add link to the documentation repo!***

To build dummy loadable library execute:

    cd libraries/dummy
    mkdir build
    cd build
    
    cmake -DRIMAGE_COMMAND="/path/to/rimage" -DSIGNING_KEY="/path/to/signing/key.pem" ..
    cmake --build .

Here RIMAGE_COMMAND is path to rimage executable binary, SIGNING_KEY is path to
signing key for rimage.

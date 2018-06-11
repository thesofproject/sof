====================================
Building & Running Cmocka Unit Tests
====================================

***************
Prerequisites
***************

You need `cmocka <https://cmocka.org/>`_ to run unit tests.

For building native libraries you can just use package manager to install it, for example with apt:

.. code-block:: bash

	sudo apt install libcmocka-dev

If you want to use `custom version of cmocka <Preparing cmocka package_>`_ (for example for cross-compilation) then you have to provide path to cmocka built for chosen architecture using **--with-cmocka-prefix** option.

*********************
Enabling unit tests
*********************

In order to build and run unit tests you need to provide path to cmocka using **--with-cmocka-prefix** option of the configure script.

After configuration you can build and run all unit tests by typing:

.. code-block:: bash

	make check

There is no need for reconfiguration when cmocka path is set in configuration script. You can use **make** for building normal APL Binary and **make check** to build and run unit tests.


********************************
Example: Running tests for APL
********************************

In order to build tests for APL platform you have to use `custom version of cmocka <Preparing cmocka package_>`_.
You can run **./configure** script with almost the same parameters as for building usual APL binary - just add **--with-cmocka-prefix=<path to cmocka>**, like:

.. code-block:: bash

	./autogen.sh
	./configure --with-arch=xtensa --with-platform=apollolake --with-dsp-core=$XTENSA_CORE --with-root-dir=$CONFIG_PATH/xtensa-elf --host=xtensa-bxt-elf --with-meu=$MEU_PATH --with-key=$PRIVATE_KEY_PATH CC=xt-xcc OBJCOPY=xt-objcopy OBJDUMP=xt-objdump --with-cmocka-prefix=/home/admin/cminstall_apl_2017_8/
	make check

************************
Preparing cmocka package
************************

1. Build cmocka with static library on:

	.. code-block:: bash

		cmake <cmocka src dir> -DWITH_STATIC_LIB=ON

	In order to build cmocka with xt-xcc to be linked with a DSP binary code, you need two things:

	a. add another option -DCMAKE_C_COMPILER=xt-xcc
	b. play with cmocka build scripts to disable building of shared library


2. Mkdir for package to be referenced by main sof build script and copy required files there:

	.. code-block:: bash

		mkdir /home/<you>/cminstall
		mkdir /home/<you>/cminstall/include
		mkdir /home/<you>/cminstall/lib

		cp cmocka.h /home/<you>/cminstall/include
		cp libcmocka.a /home/<you>/cminstall/lib

3. Use the target location for cmocka files when invoking *configure* script:

	.. code-block:: bash

		./configure --with-cmocka-prefix=/home/<you>/cminstall ...

************************
Notes
************************

#. It is recommended to use **make check -j** option while running tests that use xt-run to speed up tests significantly by running multiple instances of xt-run simulator (it also speeds up build if you have many unit tests).

#. When you switch platforms for example from native to APL, please use **make clean**, otherwise make will not build binaries for new platform and your tests will fail.

#. To speed up development of new unit tests you can run specific tests like:
	.. code-block:: bash

		make check check_PROGRAMS="testname1 testname2"


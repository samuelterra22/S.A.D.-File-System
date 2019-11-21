# S.A.D. File System

Use CMake to check dependencies, setup environment and generate Makefiles.
```shell script
cmake -DCMAKE_BUILD_TYPE=Debug .
```

Use CMake generated Makefiles is now able to build the project.
```shell script
make -j
```

Before doing anything we need a mountpoint, so let's create the directory where the filesystem will be mounted:
```shell script
mkdir /tmp/example
```

and then, mount the filesystem:
```shell script
./bin/new-fuse -d -s -f /tmp/example
```

As you may notice, we mounted the filesystem with three arguments which are:

- d: enable debugging
- s: run single threaded
- f: stay in foreground

Now check that it has been mounted:
```shell script
ls -al /tmp/example/
mount | grep fuse-example
```

If necessary, you can unmount the file system as follows:
```shell script
fusermount -u /tmp/example
```
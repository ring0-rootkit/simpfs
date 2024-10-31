# simpfs

Simple user space filesystem implemented with (libFUSE)[https://github.com/libfuse/libfuse]

File system stores all data in `data` file alongsied folder it is mounted in.

# Usage

## Compile and run

To compile source code you will need all dependencies from 
(libFUSE)[https://github.com/libfuse/libfuse] repo
or you can use prebuilt binaries that you can find in (releases)[https://github.com/ring0-rootkit/simpfs/releases/tag/1.0]

```bash
mkdir fs // directory where file system will be mounted
make main
./main fs
```

Now file system is mounted in `fs` dir 


```bash
sudo umount fs -f
```

## Check data dump

To check how data is stored inside run inside directory where `data` file is located
```bash
make dump
./dump [-v]
```

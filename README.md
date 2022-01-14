# UCUTag - tag file system
**Fuse-based tag-oriented file system**

![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Fuse](https://img.shields.io/badge/Fuse-%2300599C.svg?style=for-the-badge&color=0f4b4f)
![MongoDB](https://img.shields.io/badge/MongoDB-%234ea94b.svg?style=for-the-badge&logo=mongodb&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-%23008FBA.svg?style=for-the-badge&logo=cmake&logoColor=white)
![Arch](https://img.shields.io/badge/Arch%20Linux-1793D1?logo=arch-linux&logoColor=fff&style=for-the-badge)

## Authors:
[Tsapiv Volodymyr](https://github.com/Tsapiv)
[Hilei Pavlo](https://github.com/Pavlik1400)
[Pankevych Yevhen](https://github.com/yewhenp)

## Project description
This project is our OS course project at [APPS UCU](https://apps.ucu.edu.ua/).

Tag-oriented file system means that instead of regular directories, we use tags. Just like tags you use in Instagram, Telegram or other social network.

You can create files just like regular files, create tags just like regular directory. Then after you create file-tag association, you can search for this file with tag. You can associate many tags with files, and use same tag for different files.

The benefits of this file system association is more natural and convenient way of file search: 
- User don't usually remember exact path to the file, but remember different keywords about it. When filtering with tags, ordere doesn't matter, and you don't have to specify all tags associated with this file. For example instead of "/home/username/Documents/studying/year3/os/lab10" on hierarchical filesystem, you can just filter files like that "/studying/os/lab10" or "lab10/year3/Documents" with tags on tag file system
- Speed. To find file on hierarchical file system you'll have to goo through all files on the computer, which takes linear time (O(n)), but on tag file system, this takes ~ O(log(n)) time on out file system (assuming you're using ext4, that uses B+ tree to find files in directory). 

## How to get ucutag?
### Install with [AUR](https://aur.archlinux.org/packages/ucutag/)
```bash
yay ucutag
```

### Compile from sources
**Prerequisites**
- cmake 3.15+
- GCC 11
- boost
- mongo-cxx-driver
- mongodb v5
- fuse v2

**Install prerequisites on ArchLinux:**
```bash
yay gcc cmake boost boost-libs fuse2 mongo-cxx-driver mongodb
```

**compile**
```bash
./compile.sh
cd build
sudo make install
sudo systemctl enable --now mongodb.service
```
## Usage
export UCUTAG_FILE_DIR enviroment veriable to use it as a "trash" diretory, where ucutag stores actual files. You can change it to "create" another file system. Make sure you have all permissions to it. Default is /opt/ucutag/files

**NOTE!** - mountpoint should be absolute path

Mount file system:
```bash
ucutag /path/to/mountpoint
```

Umount file system:
```bash
fusermount -u /path/to/mountpoint
```

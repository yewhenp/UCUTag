import os
import time
import argparse
import multiprocessing as mp


def main_fn_write(data):
    with open(data, "w") as file:
        file.write(str("hello"))

def main_fn_access(data):
    if not os.path.isfile(data):
        print("Access error: ", data)

def main_fn_remove(data):
    os.remove(data)

def main_fn_search(data, fs_type, dir_root):
    if fs_type != "tag":
        result = []
        for root, dir, files in os.walk(dir_root):
            if data in files:
                result.append(os.path.join(root, data))
        if len(result) != 1:
            print("Search error: ", data, "got ", result)
    else:
        result = os.path.isfile(f"{dir_root}/{data}")
        if not result:
            print("Search error: ", data)


class FSTester:
    def __init__(self, dir_root, tree_height=2, tree_dir_width=2, tree_file_width=5, fs_type="tag"):
        self.dir_root = dir_root
        self.tree_height = tree_height
        self.tree_dir_width = tree_dir_width
        self.tree_file_width = tree_file_width
        self.fs_type = fs_type
        self.file_pathes = []
        self.filenames = []
        self.dir_ind = 0
        self.file_ind = 0
        self.num_workers = 6

    def create_dirs_rec(self, path, cur_height=0):
        if cur_height >= self.tree_height:
            return
        for i in range(self.tree_dir_width):
            cur_path = f"{path}/dir{self.dir_ind}"
            if self.fs_type != "tag":
                os.mkdir(cur_path)
            else:
                os.mkdir(f"{self.dir_root}/dir{self.dir_ind}")
            self.dir_ind += 1

            for j in range(self.tree_file_width):
                file_path = f"{cur_path}/file{self.file_ind}.txt"
                self.file_pathes.append(file_path)
                self.filenames.append(f"file{self.file_ind}.txt")
                self.file_ind += 1

            self.create_dirs_rec(cur_path, cur_height + 1)

    def create_files(self):
        pool = mp.Pool(self.num_workers)
        [pool.apply(main_fn_write, args=(row,)) for row in self.file_pathes]
        pool.close()

    def access_all_files(self):
        pool = mp.Pool(self.num_workers)
        [pool.apply(main_fn_access, args=(row,)) for row in self.file_pathes]
        pool.close()

    def delete_files(self):
        pool = mp.Pool(self.num_workers)
        [pool.apply(main_fn_remove, args=(row,)) for row in self.file_pathes]
        pool.close()

    def search_files(self):
        pool = mp.Pool(self.num_workers)
        [pool.apply(main_fn_search, args=(row, self.fs_type, self.dir_root,)) for row in self.filenames]
        pool.close()

    def delete_dirs(self, path, cur_height=0):
        if cur_height >= self.tree_height:
            return
        for i in range(self.tree_dir_width):
            cur_path = f"{path}/dir{self.dir_ind}"
            if self.fs_type == "tag":
                os.rmdir(f"{self.dir_root}/dir{self.dir_ind}")
            self.dir_ind += 1

            self.delete_dirs(cur_path, cur_height + 1)

            if self.fs_type != "tag":
                os.rmdir(cur_path)

    def count_time(self):
        start = time.time()
        self.create_dirs_rec(self.dir_root)
        create_dir_time = -start + time.time()
        print("Create dirs time:", create_dir_time)

        self.create_files()
        create_files_time = -start - create_dir_time + time.time()
        print("Create files time:", create_files_time)

        self.access_all_files()
        access_files_time = -start - create_dir_time - create_files_time + time.time()
        print("Access files time:", access_files_time)

        self.search_files()
        search_files_time = -start - create_dir_time - create_files_time - access_files_time + time.time()
        print("Search files time:", search_files_time)

        self.delete_files()
        delete_files_time = -start - create_dir_time - create_files_time - access_files_time - search_files_time + time.time()
        print("Delete files time:", delete_files_time)

        self.dir_ind = 0
        self.delete_dirs(self.dir_root)
        delete_dirs_time = -start - create_dir_time - create_files_time - access_files_time - search_files_time - delete_files_time + time.time()
        print("Delete dirs time:", delete_dirs_time)

        return [create_dir_time, create_files_time, access_files_time, search_files_time, delete_files_time, delete_dirs_time]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="FS benchmark")
    parser.add_argument("--root", dest="dir_root", required=True, type=str)
    parser.add_argument("--tree-height", dest="tree_height", default=2, type=int)
    parser.add_argument("--tree-dir-width", dest="tree_dir_width", default=2, type=int)
    parser.add_argument("--tree-file-width", dest="tree_file_width", default=5, type=int)
    parser.add_argument("--fs-type", dest="fs_type", default="tag", type=str)
    args = parser.parse_args()

    fs_tester = FSTester(args.dir_root, args.tree_height, args.tree_dir_width, args.tree_file_width, args.fs_type)
    fs_tester.count_time()

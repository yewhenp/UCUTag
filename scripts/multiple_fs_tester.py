import json
import argparse
from fs_tester import FSTester


def main():
    parser = argparse.ArgumentParser(description="FS benchmark")
    parser.add_argument("--root", dest="dir_root", required=True, type=str)
    parser.add_argument("--tree-dir-width", dest="tree_dir_width", default=3, type=int)
    parser.add_argument("--fs-type", dest="fs_type", default="ext2", type=str)
    args = parser.parse_args()

    create_dir_time_all, create_files_time_all, access_files_time_all, \
        search_files_time_all, delete_files_time_all, delete_dirs_time_all = [], [], [], [], [], []
    tree_heights = [2, 3, 4, 5]
    tree_file_widths = [4, 5, 6, 7, 8]
    for tree_height in tree_heights:
        for tree_file_width in tree_file_widths:
            print(f"Running with tree_height = {tree_height} and tree_file_width = {tree_file_width}")

            fs_tester = FSTester(args.dir_root, tree_height, args.tree_dir_width, tree_file_width, args.fs_type)
            create_dir_time, create_files_time, access_files_time, \
                search_files_time, delete_files_time, delete_dirs_time = fs_tester.count_time()

            create_dir_time_all.append(create_dir_time)
            create_files_time_all.append(create_files_time)
            access_files_time_all.append(access_files_time)
            search_files_time_all.append(search_files_time)
            delete_files_time_all.append(delete_files_time)
            delete_dirs_time_all.append(delete_dirs_time)

            print()

    with open(f"{args.fs_type}_rez.json", "w") as file:
        json.dump({
            "tree_heights": tree_heights,
            "tree_file_widths": tree_file_widths,
            "create_dir_time_all": create_dir_time_all,
            "create_files_time_all": create_files_time_all,
            "access_files_time_all": access_files_time_all,
            "search_files_time_all": search_files_time_all,
            "delete_files_time_all": delete_files_time_all,
            "delete_dirs_time_all": delete_dirs_time_all,
        }, file)


if __name__ == '__main__':
    main()

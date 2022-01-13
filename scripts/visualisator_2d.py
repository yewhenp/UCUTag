import json
import argparse
import numpy as np
import matplotlib.pyplot as plt


def animator(tree_heights, tree_file_widths, ext_data, tag_data, tag_mem_data, data_key):
    file_sizes = np.asarray([273, 720, 2541, 7644])

    tag_mem_slice = np.asarray(tag_mem_data[data_key]).reshape((len(tree_heights), len(tree_file_widths)))[:,2] / file_sizes
    tag_slice = np.asarray(tag_data[data_key]).reshape((len(tree_heights), len(tree_file_widths)))[:,2] / file_sizes
    ext_slice = np.asarray(ext_data[data_key]).reshape((len(tree_heights), len(tree_file_widths)))[:,2] / file_sizes

    print(tag_slice)

    plt.plot(tree_heights, tag_mem_slice, color='r', label='Tag fs on memory')
    plt.plot(tree_heights, tag_slice, color='g', label='Tag fs')
    plt.plot(tree_heights, ext_slice, color='b', label='Ext')
    plt.title(data_key)
    plt.legend()
    plt.savefig(f'{data_key}.png')
    plt.cla()
    plt.clf()


def main():
    parser = argparse.ArgumentParser(description="FS benchmark")
    parser.add_argument("--path-ext", dest="path_ext", default="scripts/ext4_rez.json", type=str)
    parser.add_argument("--path-tag", dest="path_tag", default="scripts/tag_rez.json", type=str)
    parser.add_argument("--path-tag-mem", dest="path_tag_mem", default="scripts/tag_rez_mem.json", type=str)
    args = parser.parse_args()

    with open(args.path_ext, "r") as file:
        ext_data = json.load(file)
    with open(args.path_tag, "r") as file:
        tag_data = json.load(file)
    with open(args.path_tag_mem, "r") as file:
        tag_mem_data = json.load(file)


    tree_heights = [3, 4, 5, 6]
    tree_file_widths = [5, 6, 7, 8]
    tree_heights_all = []
    tree_file_widths_all = []
    for tree_height in tree_heights:
        for tree_file_width in tree_file_widths:
            tree_heights_all.append(tree_height)
            tree_file_widths_all.append(tree_file_width)

    for data_key in ["create_dir_time_all", "create_files_time_all", "access_files_time_all",
                     "search_files_time_all", "delete_files_time_all", "delete_dirs_time_all"]:
        animator(tree_heights, tree_file_widths, ext_data, tag_data, tag_mem_data, data_key)


if __name__ == '__main__':
    main()

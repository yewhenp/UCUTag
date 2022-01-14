import json
import argparse
import numpy as np
import plotly.graph_objects as go


def animator(tree_heights, tree_file_widths, ext_data, tag_data, data_key):
    fig = go.Figure(data=[
        go.Surface(z=np.asarray(ext_data[data_key]).reshape((len(tree_heights), len(tree_file_widths))),
                   x=tree_heights, y=tree_file_widths, colorscale='Electric'),
        go.Surface(z=np.asarray(tag_data[data_key]).reshape((len(tree_heights), len(tree_file_widths))),
                   x=tree_heights, y=tree_file_widths, opacity=0.9, colorscale='Viridis'),
    ])
    fig.update_layout(title=data_key)
    fig.show()


def main():
    parser = argparse.ArgumentParser(description="FS benchmark")
    parser.add_argument("--path-ext", dest="path_ext", default="new_rez/ext2_rez.json", type=str)
    parser.add_argument("--path-tag", dest="path_tag", default="new_rez/tag_rez.json", type=str)
    args = parser.parse_args()

    with open(args.path_ext, "r") as file:
        ext_data = json.load(file)
    with open(args.path_tag, "r") as file:
        tag_data = json.load(file)

    tree_heights = [2, 3, 4, 5]
    tree_file_widths = [4, 5, 6, 7, 8]
    tree_heights_all = []
    tree_file_widths_all = []
    for tree_height in tree_heights:
        for tree_file_width in tree_file_widths:
            tree_heights_all.append(tree_height)
            tree_file_widths_all.append(tree_file_width)

    for data_key in ["create_dir_time_all", "create_files_time_all", "access_files_time_all",
                     "search_files_time_all", "delete_files_time_all", "delete_dirs_time_all"]:
        animator(tree_heights, tree_file_widths, ext_data, tag_data, data_key)


if __name__ == '__main__':
    main()

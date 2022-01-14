import os

import pymongo


class DBTagTester:
    inodeToTag = "inodeToTag"
    inodetoFilename = "inodetoFilename"
    tagToInode = "tagToInode"
    tags = "tags"

    def __init__(self, mountpoint, db_name):
        client = pymongo.MongoClient("mongodb://localhost:27017/")
        self.db = client[db_name]

        self.mountpoint = mountpoint
        self.new_tags = ["tag_test_1", "tag_test_2", "tag_test_3"]
        self.new_files = ["tag_test_1/tag_test_2/file_test_1",
                          "tag_test_1/tag_test_3/file_test_2",
                          "tag_test_1/file_test_3",
                          "tag_test_1/tag_test_2/tag_test_3/file_test_4"]

    def test_create_tags(self):
        before_data = list(self.db[self.tags].find())
        for tag in self.new_tags:
            os.mkdir(self.mountpoint + tag)
        after_data = list(self.db[self.tags].find())

        assert len(before_data) + len(self.new_tags) == len(after_data), str(before_data) + str(after_data)
        for tag in self.new_tags:
            flag_tag_found = False
            flag_tag_empty = False
            for entry in after_data:
                if entry["tagname"] == tag:
                    flag_tag_found = True
                    tags_inodes = list(self.db[self.tagToInode].find({"_id": entry["_id"]}))
                    if len(tags_inodes) == 1 and len(tags_inodes[0]["inodes"]) == 0:
                        flag_tag_empty = True
            assert flag_tag_found
            # assert flag_tag_empty

        print("create tags ok")

    def test_create_files(self):
        before_data_inodetoFilename = list(self.db[self.inodetoFilename].find())
        before_data_inodeToTag = list(self.db[self.inodeToTag].find())
        for file in self.new_files:
            with open(self.mountpoint + file, "w") as filee:
                filee.write(str("hello"))
        after_data_inodetoFilename = list(self.db[self.inodetoFilename].find())
        after_data_inodeToTag = list(self.db[self.inodeToTag].find())

        assert len(before_data_inodetoFilename) + len(self.new_files) == len(after_data_inodetoFilename), str(
            before_data_inodetoFilename) + str(after_data_inodetoFilename)
        assert len(before_data_inodeToTag) + len(self.new_files) == len(after_data_inodeToTag), str(
            before_data_inodeToTag) + str(after_data_inodeToTag)

        for file in self.new_files:
            flag_file_found = False
            for entry in after_data_inodetoFilename:
                if entry["filename"] == file.split("/")[-1]:
                    flag_file_found = True
                    file_tags = list(self.db[self.inodeToTag].find({"_id": entry["_id"]}))
                    assert len(file_tags) == 1
                    file_tags = file_tags[0]
                    for tag in file.split("/")[:-1]:
                        tag_id = self.db[self.tags].find_one({"tagname": tag})["_id"]
                        assert tag_id in file_tags["tags"]
            assert flag_file_found

        print("create files ok")

    def test_delete_files(self):
        before_data_inodetoFilename = list(self.db[self.inodetoFilename].find())
        before_data_inodeToTag = list(self.db[self.inodeToTag].find())
        for file in self.new_files:
            os.remove(self.mountpoint + file)
        after_data_inodetoFilename = list(self.db[self.inodetoFilename].find())
        after_data_inodeToTag = list(self.db[self.inodeToTag].find())

        assert len(before_data_inodetoFilename) - len(self.new_files) == len(after_data_inodetoFilename)
        assert len(before_data_inodeToTag) - len(self.new_files) == len(after_data_inodeToTag)

        print("delete files ok")

    def test_delete_tags(self):
        before_data = list(self.db[self.tags].find())
        for tag in self.new_tags:
            os.rmdir(self.mountpoint + tag)
        after_data = list(self.db[self.tags].find())
        assert len(before_data) - len(self.new_tags) == len(after_data)

        print("delete tags ok")


if __name__ == '__main__':
    tester = DBTagTester("/home/vtsap/Documents/OS/tagfs/UCUTag/mountpoint/", "ucutag_home_vtsap_Documents_OS_tagfs_UCUTag_files")
    tester.test_create_tags()
    tester.test_create_files()
    tester.test_delete_files()
    tester.test_delete_tags()

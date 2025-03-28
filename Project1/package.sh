mkdir project_1_omada_3672_3771_3796
cp -r project1_find_roots/ project_1_omada_3672_3771_3796/
cp -r project1_module/ project_1_omada_3672_3771_3796/
cp -r sysfs_module/ project_1_omada_3672_3771_3796/
cp patch_1 project_1_omada_3672_3771_3796/
cp README.txt project_1_omada_3672_3771_3796/
tar -cvf project_1_omada_3672_3771_3796.tar project_1_omada_3672_3771_3796/
rm -rf project_1_omada_3672_3771_3796/
bzip2 project_1_omada_3672_3771_3796.tar
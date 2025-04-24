
echo "~~~~~~~~~~~~~~~~~~~Run Script~~~~~~~~~~~~~~~~~~~"
# if ! [ -w '/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/PAGExporter.plugin' ]; then
# 	${PROJECT_DIR}/scripts/takeControl
# fi

echo ${SYMROOT}/Debug

rm -r -f /Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/Grabba.plugin
cp -r -f ${SYMROOT}/Debug/Grabba.plugin /Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/

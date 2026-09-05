#! /bin/bash

# This script refactors this plugin into a new plugin with a name of your choice.
# To rename the plugin to "catsarebest", run `bash make-new-plugin.sh catsarebest`.

newname="$1"
Newname="$(echo "${newname^}")"
NEWNAME="$(echo "${newname^^}")"

grep -rl indexed_parallel_lines . | grep -v .git | while read name; do
  sed -e "s+indexed_parallel_lines+$newname+g" -i "$name";
done

grep -rl Indexed_parallel_lines . | grep -v .git | while read name; do  
  sed -e "s+Indexed_parallel_lines+$Newname+g" -i "$name";
done 

grep -rl INDEXED_PARALLEL_LINES . | grep -v .git | while read name; do  
  sed -e "s+INDEXED_PARALLEL_LINES+$NEWNAME+g" -i "$name";
done 

find . -name "*indexed_parallel_lines*" | grep -v .git | while read name; do
  mv "$name" "$(echo "$name" | sed -e "s+indexed_parallel_lines+$newname+g")"
done

find . -name "*Indexed_parallel_lines*" | grep -v .git | while read name; do
  mv "$name" "$(echo "$name" | sed -e "s+Indexed_parallel_lines+$Newname+g")"
done

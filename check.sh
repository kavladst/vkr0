set -e

FILE=txtbig.txt

FILEGZ=$FILE.gz
COPYGZ=copy$FILEGZ

cp $FILEGZ $COPYGZ

if [ "$(diff $FILE <(gzip -c -d $COPYGZ))" == "" ]; then
  echo 'Great!'
else
  echo 'Wrong'
fi

rm $COPYGZ

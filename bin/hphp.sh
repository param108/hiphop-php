if [ "a$HPHP_HOME" = "a" ]
then
echo "please set HPHP_HOME environmental variable"
exit
fi
$HPHP_HOME/src/hphp/hphp --keep-tempdir=1 --log=3  $*


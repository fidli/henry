call rsync -c -e "ssh -i ssh/id_rsa" -r sources pi@henry:henry
call rsync -c -L -e "ssh -i ssh/id_rsa" -r baselib pi@henry:henry
call rsync -c -e "ssh -i ssh/id_rsa" build.sh pi@henry:henry
call rsync -c -e "ssh -i ssh/id_rsa" Makefile pi@henry:henry
ssh -i ssh/id_rsa pi@henry "cd henry;./build.sh"
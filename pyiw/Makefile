# jeremy@emperorlinux.com

NAME  = pyiw
GCC   = gcc -Wall
FLAGS = -I/usr/include/python2.5 -I/home/cubicool/local/include
LIBS  = -liw -L/home/cubicool/local/lib

static: pyiw.c
	@echo "Statically Building/Linking $(NAME)"
	@$(GCC) $(FLAGS) -Istaticlibiw $(<) -shared -o $(NAME).so staticlibiw/libiw.a

dynamic: pyiw.c
	@echo "Dynamically Building/Linking $(NAME)"
	@$(GCC) $(FLAGS) $(LIBS) $(<) -shared -o $(NAME).so

clean:
	@rm -rf *.o
	@rm -rf $(NAME).so
	@rm -rf $(NAME).so.t*

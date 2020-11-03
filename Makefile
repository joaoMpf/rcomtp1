# all: read write

# read:
# 	cc -g noncanonical.c
# write:	
# 	cc -g writenoncanonical.c

# clean:
# 	rm write && rm read


all: read write

read:
	gcc -o b reader.c reader.h
write:	
	gcc -o a writer.c writer.h
clean:
	rm a && rm b

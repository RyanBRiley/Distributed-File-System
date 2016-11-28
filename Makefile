make:
	-gcc client/dfc.c -o client/dfc
	-gcc server/dfs.c -o server/dfs
clean:
	-rm client/dfc
	-rm server/dfs
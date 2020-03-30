ifdef CONFIG_SPL_BUILD
obj-y += spl.o
else
obj-y += board.o
endif

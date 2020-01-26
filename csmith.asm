	section	.init
	public	__start
__start:
	call	_main
	halt
	db "exit", 0

	extern	_main

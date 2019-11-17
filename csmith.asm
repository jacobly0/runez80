	.ref _main

	.segment init
	call	_main
	halt
	db "exit", 0

	.text
	.file	"test_bitcode.ll"
	.globl	mult                    # -- Begin function mult
	.p2align	4, 0x90
	.type	mult,@function
mult:                                   # @mult
	.cfi_startproc
# %bb.0:
	imull	%esi, %edi
	movl	%edi, %eax
	retq
.Lfunc_end0:
	.size	mult, .Lfunc_end0-mult
	.cfi_endproc
                                        # -- End function

	.section	".note.GNU-stack","",@progbits
	.addrsig

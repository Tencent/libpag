resource 'PiPL' (16000) {
	{	/* array properties: 7 elements */
		/* [1] */
		Kind {
			AEGP
		},
		/* [2] */
		Name {
			"ProjDumper"
		},
		/* [3] */
		Category {
			"General Plugin"
		},
		/* [4] */
		Version {
			196608
		},
		/* [5] */
#ifdef AE_OS_WIN
	#ifdef AE_PROC_INTELx64
		CodeWin64X86 {
			"GPMain_ProjDumper"
		},
	#endif
#else	
CodePowerPC {
			0,
			0,
			""
		},
#endif
		/* [6] */
		AE_Reserved_Info {
			0
		},
		/* [7] */
		AE_Reserved {
			0
		}
	}
};


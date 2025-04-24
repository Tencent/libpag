#ifndef _PT_ERR_H_
#define _PT_ERR_H_


#define PT_XTE_START {A_Err _err = A_Err_NONE; try {

#define PT_CATCH_PART1		}	catch (long _tmp_err) { _err = _tmp_err; } \
								catch(std::bad_alloc&) { _err = A_Err_ALLOC; }
#define PT_CATCH_PART2		if (_err) {
#define PT_XTE_CATCH		PT_CATCH_PART1 PT_CATCH_PART2
#define PT_ENDTRY			} if (_err) throw ((long) _err); }

#define PT_NO_PROPAGATE		_err = A_Err_NONE
#define PT_XTE_ENDTRY(ERR)	ERR = _err; PT_NO_PROPAGATE; PT_ENDTRY

#define PT_XTE(EXPR) PT_XTE_START { EXPR; } PT_XTE_CATCH { ; } PT_XTE_ENDTRY(err)

#define PT_XTE_CATCH_RETURN_ERR		PT_XTE_CATCH A_Err retErr; retErr = _err; PT_NO_PROPAGATE; return retErr; }};	return A_Err_NONE;

#define PT_ETX(EXPR) { A_Err _res = (EXPR); if(_res != A_Err_NONE) throw _res;}

#define PT_ERR(EXPR) { if (!err) {	err = (EXPR);	}	}

#endif
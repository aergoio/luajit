/*
** Bytecode instruction format.
** Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_BC_H
#define _LJ_BC_H

#include "lj_def.h"
#include "lj_arch.h"
#include "lj_gas.h"

/* Bytecode instruction format, 32 bit wide, fields of 8 or 16 bit:
**
** +----+----+----+----+
** | B  | C  | A  | OP | Format ABC
** +----+----+----+----+
** |    D    | A  | OP | Format AD
** +--------------------
** MSB               LSB
**
** In-memory instructions are always stored in host byte order.
*/

/* Operand ranges and related constants. */
#define BCMAX_A		0xff
#define BCMAX_B		0xff
#define BCMAX_C		0xff
#define BCMAX_D		0xffff
#define BCBIAS_J	0x8000
#define NO_REG		BCMAX_A
#define NO_JMP		(~(BCPos)0)

/* Macros to get instruction fields. */
#define bc_op(i)	((BCOp)((i)&0xff))
#define bc_a(i)		((BCReg)(((i)>>8)&0xff))
#define bc_b(i)		((BCReg)((i)>>24))
#define bc_c(i)		((BCReg)(((i)>>16)&0xff))
#define bc_d(i)		((BCReg)((i)>>16))
#define bc_j(i)		((ptrdiff_t)bc_d(i)-BCBIAS_J)

/* Macros to set instruction fields. */
#define setbc_byte(p, x, ofs) \
  ((uint8_t *)(p))[LJ_ENDIAN_SELECT(ofs, 3-ofs)] = (uint8_t)(x)
#define setbc_op(p, x)	setbc_byte(p, (x), 0)
#define setbc_a(p, x)	setbc_byte(p, (x), 1)
#define setbc_b(p, x)	setbc_byte(p, (x), 3)
#define setbc_c(p, x)	setbc_byte(p, (x), 2)
#define setbc_d(p, x) \
  ((uint16_t *)(p))[LJ_ENDIAN_SELECT(1, 0)] = (uint16_t)(x)
#define setbc_j(p, x)	setbc_d(p, (BCPos)((int32_t)(x)+BCBIAS_J))

/* Macros to compose instructions. */
#define BCINS_ABC(o, a, b, c) \
  (((BCIns)(o))|((BCIns)(a)<<8)|((BCIns)(b)<<24)|((BCIns)(c)<<16))
#define BCINS_AD(o, a, d) \
  (((BCIns)(o))|((BCIns)(a)<<8)|((BCIns)(d)<<16))
#define BCINS_AJ(o, a, j)	BCINS_AD(o, a, (BCPos)((int32_t)(j)+BCBIAS_J))

/* Bytecode instruction definition. Order matters, see below.
**
** (name, filler, Amode, Bmode, Cmode or Dmode, metamethod)
**
** The opcode name suffixes specify the type for RB/RC or RD:
** V = variable slot
** S = string const
** N = number const
** P = primitive type (~itype)
** B = unsigned byte literal
** M = multiple args/results
*/
#define BCDEF(_) \
  /* Comparison ops. ORDER OPR. */ \
  _(ISLT,	var,	___,	var,	lt,     GAS_FAST) \
  _(ISGE,	var,	___,	var,	lt,     GAS_FAST) \
  _(ISLE,	var,	___,	var,	le,     GAS_FAST) \
  _(ISGT,	var,	___,	var,	le,     GAS_FAST) \
  \
  _(ISEQV,	var,	___,	var,	eq,     GAS_FAST) \
  _(ISNEV,	var,	___,	var,	eq,     GAS_FAST) \
  _(ISEQS,	var,	___,	str,	eq,     GAS_FAST) \
  _(ISNES,	var,	___,	str,	eq,     GAS_FAST) \
  _(ISEQN,	var,	___,	num,	eq,     GAS_FAST) \
  _(ISNEN,	var,	___,	num,	eq,     GAS_FAST) \
  _(ISEQP,	var,	___,	pri,	eq,     GAS_FAST) \
  _(ISNEP,	var,	___,	pri,	eq,     GAS_FAST) \
  \
  /* Unary test and copy ops. */ \
  _(ISTC,	dst,	___,	var,	___,    GAS_FASTEST) \
  _(ISFC,	dst,	___,	var,	___,    GAS_FASTEST) \
  _(IST,	___,	___,	var,	___,    GAS_FASTEST) \
  _(ISF,	___,	___,	var,	___,    GAS_FASTEST) \
  _(ISTYPE,	var,	___,	lit,	___,    GAS_FASTEST) \
  _(ISNUM,	var,	___,	lit,	___,    GAS_FASTEST) \
  \
  /* Unary ops. */ \
  _(MOV,	dst,	___,	var,	___,    GAS_FAST) \
  _(NOT,	dst,	___,	var,	___,    GAS_FASTEST) \
  _(UNM,	dst,	___,	var,	unm,    GAS_FASTEST) \
  _(LEN,	dst,	___,	var,	len,    GAS_MID) \
  \
  /* Binary ops. ORDER OPR. VV last, POW must be next. */ \
  _(ADDVN,	dst,	var,	num,	add,    GAS_FAST) \
  _(SUBVN,	dst,	var,	num,	sub,    GAS_FAST) \
  _(MULVN,	dst,	var,	num,	mul,    GAS_MID) \
  _(DIVVN,	dst,	var,	num,	div,    GAS_MID) \
  _(MODVN,	dst,	var,	num,	mod,    GAS_MID) \
  \
  _(ADDNV,	dst,	var,	num,	add,    GAS_FAST) \
  _(SUBNV,	dst,	var,	num,	sub,    GAS_FAST) \
  _(MULNV,	dst,	var,	num,	mul,    GAS_MID) \
  _(DIVNV,	dst,	var,	num,	div,    GAS_MID) \
  _(MODNV,	dst,	var,	num,	mod,    GAS_MID) \
  \
  _(ADDVV,	dst,	var,	var,	add,    GAS_FAST) \
  _(SUBVV,	dst,	var,	var,	sub,    GAS_FAST) \
  _(MULVV,	dst,	var,	var,	mul,    GAS_MID) \
  _(DIVVV,	dst,	var,	var,	div,    GAS_MID) \
  _(MODVV,	dst,	var,	var,	mod,    GAS_MID) \
  \
  _(POW,	dst,	var,	var,	pow,    GAS_SLOW) \
  _(CAT,	dst,	rbase,	rbase,	concat, GAS_SLOW) \
  \
  /* Constant ops. */ \
  _(KSTR,	dst,	___,	str,	___,    GAS_FASTEST) \
  _(KCDATA,	dst,	___,	cdata,	___,    GAS_FASTEST) \
  _(KSHORT,	dst,	___,	lits,	___,    GAS_FASTEST) \
  _(KNUM,	dst,	___,	num,	___,    GAS_FASTEST) \
  _(KPRI,	dst,	___,	pri,	___,    GAS_FASTEST) \
  _(KNIL,	base,	___,	base,	___,    GAS_FASTEST) \
  \
  /* Upvalue and function ops. */ \
  _(UGET,	dst,	___,	uv,	___,        GAS_FAST) \
  _(USETV,	uv,	___,	var,	___,        GAS_FAST) \
  _(USETS,	uv,	___,	str,	___,        GAS_FAST) \
  _(USETN,	uv,	___,	num,	___,        GAS_FAST) \
  _(USETP,	uv,	___,	pri,	___,        GAS_FAST) \
  _(UCLO,	rbase,	___,	jump,	___,    GAS_FAST) \
  _(FNEW,	dst,	___,	func,	gc,     GAS_FAST) \
  \
  /* Table ops. */ \
  _(TNEW,	dst,	___,	lit,	gc,         GAS_FAST) \
  _(TDUP,	dst,	___,	tab,	gc,         GAS_SLOW) \
  _(GGET,	dst,	___,	str,	index,      GAS_SLOW) \
  _(GSET,	var,	___,	str,	newindex,   GAS_SLOW) \
  _(TGETV,	dst,	var,	var,	index,      GAS_FAST) \
  _(TGETS,	dst,	var,	str,	index,      GAS_FAST) \
  _(TGETB,	dst,	var,	lit,	index,      GAS_FAST) \
  _(TGETR,	dst,	var,	var,	index,      GAS_FAST) \
  _(TSETV,	var,	var,	var,	newindex,   GAS_FAST) \
  _(TSETS,	var,	var,	str,	newindex,   GAS_FAST) \
  _(TSETB,	var,	var,	lit,	newindex,   GAS_FAST) \
  _(TSETM,	base,	___,	num,	newindex,   GAS_FAST) \
  _(TSETR,	var,	var,	var,	newindex,   GAS_FAST) \
  \
  /* Calls and vararg handling. T = tail call. */ \
  _(CALLM,	base,	lit,	lit,	call,      GAS_EXT) \
  _(CALL,	base,	lit,	lit,	call,      GAS_EXT) \
  _(CALLMT,	base,	___,	lit,	call,      GAS_EXT) \
  _(CALLT,	base,	___,	lit,	call,      GAS_EXT) \
  _(ITERC,	base,	lit,	lit,	call,      GAS_EXT) \
  _(ITERN,	base,	lit,	lit,	call,      GAS_EXT) \
  _(VARG,	base,	lit,	lit,	___,       GAS_SLOW) \
  _(ISNEXT,	base,	___,	jump,	___,       GAS_FAST) \
  \
  /* Returns. */ \
  _(RETM,	base,	___,	lit,	___,    GAS_SLOW) \
  _(RET,	rbase,	___,	lit,	___,    GAS_MID) \
  _(RET0,	rbase,	___,	lit,	___,    GAS_MID) \
  _(RET1,	rbase,	___,	lit,	___,    GAS_MID) \
  \
  /* Loops and branches. I/J = interp/JIT, I/C/L = init/call/loop. */ \
  _(FORI,	base,	___,	jump,	___,    GAS_FAST) \
  _(JFORI,	base,	___,	jump,	___,    GAS_FAST) \
  \
  _(FORL,	base,	___,	jump,	___,    GAS_FAST) \
  _(IFORL,	base,	___,	jump,	___,    GAS_FAST) \
  _(JFORL,	base,	___,	lit,	___,    GAS_FAST) \
  \
  _(ITERL,	base,	___,	jump,	___,    GAS_FAST) \
  _(IITERL,	base,	___,	jump,	___,    GAS_FAST) \
  _(JITERL,	base,	___,	lit,	___,    GAS_FAST) \
  \
  _(LOOP,	rbase,	___,	jump,	___,    GAS_FAST) \
  _(ILOOP,	rbase,	___,	jump,	___,    GAS_FAST) \
  _(JLOOP,	rbase,	___,	lit,	___,    GAS_FAST) \
  \
  _(JMP,	rbase,	___,	jump,	___,    GAS_FAST) \
  \
  /* Function headers. I/J = interp/JIT, F/V/C = fixarg/vararg/C func. */ \
  _(FUNCF,	rbase,	___,	___,	___,    GAS_ZERO) \
  _(IFUNCF,	rbase,	___,	___,	___,    GAS_ZERO) \
  _(JFUNCF,	rbase,	___,	lit,	___,    GAS_ZERO) \
  _(FUNCV,	rbase,	___,	___,	___,    GAS_ZERO) \
  _(IFUNCV,	rbase,	___,	___,	___,    GAS_ZERO) \
  _(JFUNCV,	rbase,	___,	lit,	___,    GAS_ZERO) \
  _(FUNCC,	rbase,	___,	___,	___,    GAS_ZERO) \
  _(FUNCCW,	rbase,	___,	___,	___,    GAS_ZERO)

/* Bytecode opcode numbers. */
typedef enum {
#define BCENUM(name, ma, mb, mc, mt, gas)	BC_##name,
BCDEF(BCENUM)
#undef BCENUM
  BC__MAX
} BCOp;

LJ_STATIC_ASSERT((int)BC_ISEQV+1 == (int)BC_ISNEV);
LJ_STATIC_ASSERT(((int)BC_ISEQV^1) == (int)BC_ISNEV);
LJ_STATIC_ASSERT(((int)BC_ISEQS^1) == (int)BC_ISNES);
LJ_STATIC_ASSERT(((int)BC_ISEQN^1) == (int)BC_ISNEN);
LJ_STATIC_ASSERT(((int)BC_ISEQP^1) == (int)BC_ISNEP);
LJ_STATIC_ASSERT(((int)BC_ISLT^1) == (int)BC_ISGE);
LJ_STATIC_ASSERT(((int)BC_ISLE^1) == (int)BC_ISGT);
LJ_STATIC_ASSERT(((int)BC_ISLT^3) == (int)BC_ISGT);
LJ_STATIC_ASSERT((int)BC_IST-(int)BC_ISTC == (int)BC_ISF-(int)BC_ISFC);
LJ_STATIC_ASSERT((int)BC_CALLT-(int)BC_CALL == (int)BC_CALLMT-(int)BC_CALLM);
LJ_STATIC_ASSERT((int)BC_CALLMT + 1 == (int)BC_CALLT);
LJ_STATIC_ASSERT((int)BC_RETM + 1 == (int)BC_RET);
LJ_STATIC_ASSERT((int)BC_FORL + 1 == (int)BC_IFORL);
LJ_STATIC_ASSERT((int)BC_FORL + 2 == (int)BC_JFORL);
LJ_STATIC_ASSERT((int)BC_ITERL + 1 == (int)BC_IITERL);
LJ_STATIC_ASSERT((int)BC_ITERL + 2 == (int)BC_JITERL);
LJ_STATIC_ASSERT((int)BC_LOOP + 1 == (int)BC_ILOOP);
LJ_STATIC_ASSERT((int)BC_LOOP + 2 == (int)BC_JLOOP);
LJ_STATIC_ASSERT((int)BC_FUNCF + 1 == (int)BC_IFUNCF);
LJ_STATIC_ASSERT((int)BC_FUNCF + 2 == (int)BC_JFUNCF);
LJ_STATIC_ASSERT((int)BC_FUNCV + 1 == (int)BC_IFUNCV);
LJ_STATIC_ASSERT((int)BC_FUNCV + 2 == (int)BC_JFUNCV);

/* This solves a circular dependency problem, change as needed. */
#define FF_next_N	4

/* Stack slots used by FORI/FORL, relative to operand A. */
enum {
  FORL_IDX, FORL_STOP, FORL_STEP, FORL_EXT
};

/* Bytecode operand modes. ORDER BCMode */
typedef enum {
  BCMnone, BCMdst, BCMbase, BCMvar, BCMrbase, BCMuv,  /* Mode A must be <= 7 */
  BCMlit, BCMlits, BCMpri, BCMnum, BCMstr, BCMtab, BCMfunc, BCMjump, BCMcdata,
  BCM_max
} BCMode;
#define BCM___		BCMnone

#define bcmode_a(op)	((BCMode)(lj_bc_mode[op] & 7))
#define bcmode_b(op)	((BCMode)((lj_bc_mode[op]>>3) & 15))
#define bcmode_c(op)	((BCMode)((lj_bc_mode[op]>>7) & 15))
#define bcmode_d(op)	bcmode_c(op)
#define bcmode_hasd(op)	((lj_bc_mode[op] & (15<<3)) == (BCMnone<<3))
#define bcmode_mm(op)	((MMS)(lj_bc_mode[op]>>11))

#define BCMODE(name, ma, mb, mc, mm, gas) \
  (BCM##ma|(BCM##mb<<3)|(BCM##mc<<7)|(MM_##mm<<11)),
#define BCMODE_FF	0

static LJ_AINLINE int bc_isret(BCOp op)
{
  return (op == BC_RETM || op == BC_RET || op == BC_RET0 || op == BC_RET1);
}

LJ_DATA const uint16_t lj_bc_mode[];
LJ_DATA const uint16_t lj_bc_ofs[];

#endif

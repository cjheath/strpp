#include	<cstdio>
#if !defined(PEG_H)
#define PEG_H
/*
 * Peg parser using pegular expressions (recursive descent prefix regular expressions)
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<cstdlib>
#include	<cstring>
#include	<pegexp.h>

// Forward declarations:
template<typename Source, typename Result, typename Context> class Peg;	// The Peg parser

// A Pegexp with the extensions required to be used in a Peg:
template<typename Context> class PegPegexp;

/*
 * A Peg parser is made up of a number of named Rules.
 * Each Rule is a Pegexp that returns selected results as an AST.
 */
template<
	typename PegexpT,
	int	MaxCaptureNames	// Maximum number of results that may be requested to return
>
class PegRule
{
public:
	const char*	name;
	PegexpT		expression;
	const char*	saves[MaxCaptureNames];	// These captures should be returned as the AST

	bool is_saved(const char* rulename)
	{
		for (int i = 0; i < MaxCaptureNames; i++)
		{
			if (!saves[i])
				break;
			if (0 == strcmp(saves[i], rulename))
				return true;
		}
		return false;
	}

	bool	solitary_save() const		// This Rule saves exactly one thing
	{ return saves[0] && (MaxCaptureNames <= 1 || saves[1] == 0); }
};

/*
 * Each time a Rule calls a subordinate Rule, a new Context is created.
 * The Contexts form a linked list back to the topmost rule, for diagnostic and left-recursion purposes.
 * This NoCapture context does no capturing and returns no AST.
 */
template<
	typename _Result = PegexpDefaultResult<PegexpDefaultSource>,
	int _MaxCaptureNames = 3
>
class PegContextNoCapture
{
public:
	using	Result = _Result;
	using	Source = typename Result::Source;
	static const int MaxCaptureNames = _MaxCaptureNames;
	using	PegexpT = PegPegexp<PegContextNoCapture>;
	using	Rule = PegRule<PegexpT, MaxCaptureNames>;
	using	PegT = Peg<Source, Result, PegContextNoCapture>;

	PegContextNoCapture(PegT* _peg, PegContextNoCapture* _parent, Rule* _rule, Source _origin)
	: peg(_peg)
	, capture_disabled(_parent ? _parent->capture_disabled : 0)
	, repetition_nesting(0)
	, parent(_parent)
	, rule(_rule)
	, origin(_origin)
	{}

	int		capture(PegexpPC name, int name_len, Result, bool in_repetition) { return 0; }
	int		capture_count() const { return 0; }
	void		rollback_capture(int count) {}
	int		capture_disabled;
	int		repetition_nesting;

	Result		result() const { return Result(); }

	PegT*		peg;
	PegContextNoCapture* 	parent;
	Rule*		rule;
	Source		origin;		// Location where this rule started, for detection of left-recursion
};

// Subclass the Pegexp template to extend virtual functions:
template<
	typename Context
> class PegPegexp : public Pegexp<Context>
{
	using	Rule = typename Context::Rule;
public:
	using	Result = typename Context::Result;
	using	Source = typename Result::Source;
	using	Base = Pegexp<Context>;
	using	State = PegexpState<Source>;
	using	PegT = Peg<Source, Result, Context>;
	PegPegexp(PegexpPC _pegexp_pc) : Base(_pegexp_pc) {}

	virtual bool	match_extended(State& state, Context* context)
	{
		switch (*state.pc)
		{
		case '<':
			{
			// Parse the call to a sub-rule. Note: These are not nul-terminated
			PegexpPC	rule_name = 0;		// The name of the rule being called
			PegexpPC	label = 0;		// The label assignd to the call
			PegexpPC	call_end = 0;		// Where to continue if the sub_rule succeeds
			parse_call(state.pc, rule_name, label, call_end);

			// Find the rule being called:
			PegT*		peg = context->peg;
			Rule*		sub_rule = peg->lookup(rule_name);
			if (!sub_rule)
				return false;			// Problem in the Peg definition, undefined rule

			// Open a nested Context to parse the current text with the new Pegexp:
			Context		sub_context(peg, context, sub_rule, state.text);
			State		start_state(state);	// Save for a failure exit
			state.pc = sub_rule->expression.pegexp;

#if defined(PEG_TRACE)
			context->print_path();
			printf(": calling %s /%s/ at `%.10s...`\n", sub_rule->name, state.pc, state.text.peek());
#endif

			if (!peg->recurse(sub_rule, state, &sub_context))
			{
#if defined(PEG_TRACE)
				printf("FAIL\n");
#endif
				state = start_state;

				return false;
			}

#if defined(PEG_TRACE)
			printf("MATCH %s `%.*s`\n", sub_rule->name, (int)state.text.bytes_from(start_state.text), start_state.text.peek());
			printf("continuing /%s/ text `%.10s`...\n", call_end, state.text.peek());
#endif

			// If the call is not labelled and the parent wants calls to this rule saved, do that
			if (!label)
				label = sub_rule->name;
			if (context->capture_disabled == 0	// If we're capturing
			 && context->rule->is_saved(label))	// And the parent wants it
			{
				if (sub_context.capture_count() > 0)
				{		// If the sub_context has captures, save that instead
					Result	r = sub_context.result();
					(void)context->capture(
						sub_rule->name, strlen(sub_rule->name),
						r, context->repetition_nesting > 0
					);
				}
				else
				{		// Else just save the text:
					Result	r(start_state.text, state.text);
					(void)context->capture(
						sub_rule->name, strlen(sub_rule->name),
						r, context->repetition_nesting > 0
					);
				}
			}

			// Continue after the matched text, but with the code following the call:
			state.pc = call_end;
			return true;
			}

		case '~':
		case '@':
		case '#':
		case '%':
		case '_':
		case ';':
		default:	// Control characters: (*state.pc > 0 && *state.pc < ' ')
			return Base::match_literal(state, context);
		}
	}

	// Null extension; treat extensions like literal characters
	virtual void	skip_extended(PegexpPC& pc)
	{
		if (*pc++ == '<')
			while (*pc != '\0' && *pc++ != '>')
				;
	}

protected:
	void	parse_call(PegexpPC pc, PegexpPC& rule_name, PegexpPC& label, PegexpPC& call_end)
	{
		pc++;
		rule_name = pc;

		// Find the end of the rule name in the Pegexp
		PegexpPC	brangle = strchr(pc, '>');
		call_end = brangle ? brangle+1 : pc+strlen(pc);
		if (*call_end == ':')
		{				// The call has an optional label
			label = ++call_end;
			while (isalnum(*call_end) || *call_end == '_')
                                call_end++;
			if (*call_end == ':')	// The label has the optional closing ':'
				call_end++;
		}
	}
};

template<
	typename Source = PegexpDefaultSource,
	typename _Result = PegexpDefaultResult<Source>,
	typename Context = PegContextNoCapture<_Result>
>
class Peg
{
public:
	using	Rule = typename Context::Rule;
	using	Result = _Result;
	using	State = PegexpState<Source>;
	using	PegexpT = PegPegexp<Context>;

	Peg(Rule* _rules, int _num_rule)
	: rules(_rules), num_rule(_num_rule)
	{
		// We must sort the rules by name, such that a binary search works:
		qsort(rules, num_rule, sizeof(rules[0]), [](const void* r1, const void* r2) {
			return strcmp(((const Rule*)r1)->name, ((const Rule*)r2)->name);
		});
	}

	State	parse(Source text, Result* r)
	{
		Rule*	top_rule = lookup("TOP");
		assert(top_rule);

#if defined(PEG_TRACE)
		printf(
			"Starting %s at `%.10s...` (pegexp `%s`)\n",
			top_rule->name,
			text.peek(),
			top_rule->expression.pegexp
		);
#endif

		Context	context(this, 0, top_rule, text);
		State	state(top_rule->expression.pegexp, text);
		bool ok = top_rule->expression.match_sequence(state, &context);
		if (ok && r)
			*r = context.result();
		return state;
	}

	Rule*	lookup(const char* name)
	{
		Rule*	rule = (Rule*)bsearch(
			name, rules, num_rule, sizeof(rules[0]),
			[](const void* key, const void* r2) {
				const char*	key_name = (const char*)key;
				const char*	rule_name = ((const Rule*)r2)->name;
				int		len = strlen(rule_name);

				int		cval = strncmp(key_name, rule_name, len);
				if (cval == 0 && (key_name[len] != '>' && key_name[len] != '\0'))
					cval = 1;	// This rule has a shortened version of this key
				return cval;
			}
		);
#if defined(PEG_TRACE)
		if (!rule)
			printf("failed to find rule `%.10s`\n", name);
#endif
		return rule;
	}

	bool	recurse(Rule* sub_rule, State& state, Context* context)
	{
		// Check for left recursion (infinite loop)
		for (Context* pp = context->parent; pp; pp = pp->parent)
		{
			if (state.text.bytes_from(pp->origin) > 0)
				break;	// This rule is starting ahead of pp and all its ancestors
			if (pp->rule == sub_rule)
			{
				left_recursion(state);
				return false;
			}
		}

		return sub_rule->expression.match_sequence(state, context);
	}

protected:
	Rule*		rules;
	int		num_rule;

	void	left_recursion(State state)
	{
#if defined(PEG_TRACE)
				pp->print_path();
				printf(": left recursion detected at `%.10s`\n", state.text.peek());
#endif
	}
};
#endif	// PEG_H

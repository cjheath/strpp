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
template<typename Source, typename Context> class Peg;	// The Peg parser

// A Pegexp with the extensions required to be used in a Peg:
template<typename Source, typename Result, typename Context> class PegPegexp;

/*
 * A Peg parser is made up of a number of named Rules.
 * Each Rule is a Pegexp that returns selected results as an AST.
 */
template<
	typename PegexpT,
	int	MaxSaves	// Maximum number of results that may be requested to return
>
class PegRule
{
public:
	const char*	name;
	PegexpT		expression;
	const char*	saves[MaxSaves];	// These captures should be returned as the AST

	bool is_saved(const char* rulename)
	{
		for (int i = 0; i < MaxSaves; i++)
		{
			if (!saves[i])
				break;
			if (0 == strcmp(saves[i], rulename))
				return true;
		}
		return false;
	}

	bool	solitary_save() const		// This Rule saves exactly one thing
	{ return saves[0] && (MaxSaves <= 1 || saves[1] == 0); }
};

/*
 * Each time a Rule calls a subordinate Rule, a new Context is created.
 * The Contexts form a linked list back to the topmost rule, for diagnostic and left-recursion purposes.
 * This NullCapture context does no capturing and returns no AST.
 */
template<
	typename Source = PegexpDefaultSource,
	int MaxSavesV = 2
>
class PegContextNullCapture
{
public:
	static const int MaxSaves = MaxSavesV;
	using	Result = PegexpDefaultResult<Source>;
	using	PegexpT = PegPegexp<Source, Result, PegContextNullCapture>;
	using	Rule = PegRule<PegexpT, MaxSaves>;
	using	PegT = Peg<Source, PegContextNullCapture>;

	PegContextNullCapture(PegT* _peg, PegContextNullCapture* _parent, Rule* _rule, Source _text)
	: peg(_peg)
	, capture_disabled(_parent ? _parent->capture_disabled : 0)
	, parent(_parent)
	, rule(_rule)
	, text(_text)
	{}

	int		capture(Result) { return 0; }
	int		capture_count() const { return 0; }
	void		rollback_capture(int count) {}
	int		capture_disabled;

	PegT*		peg;
	PegContextNullCapture* 	parent;
	Rule*		rule;
	Source		text;
};

// Subclass the Pegexp template to extend virtual functions:
template<
	typename Source = PegexpDefaultSource,
	typename Result = PegexpDefaultResult<Source>,
	typename Context = PegContextNullCapture<Source>
> class PegPegexp : public Pegexp<Source, Result, Context>
{
public:
	using	Base = Pegexp<Source, Result, Context>;
	using	State = PegexpState<Source>;
	PegPegexp(PegexpPC _pegexp_pc) : Base(_pegexp_pc) {}

	virtual bool	match_extended(State& state, Context* context)
	{
		switch (*state.pc)
		{
		// Extended commands:
		case '<':
			return context->peg->recurse(state, context);

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
};

template<
	typename Source = PegexpDefaultSource,
	typename Context = PegContextNullCapture<Source>
>
class Peg
{
public:
	using	Rule = typename Context::Rule;
	using	State = PegexpState<Source>;
	using	Result = PegexpDefaultResult<Source>;
	using	PegexpT = PegPegexp<Source, Result, Context>;

	Peg(Rule* _rules, int _num_rule)
	: rules(_rules), num_rule(_num_rule)
	{
		// We must sort the rules by name, such that a binary search works:
		qsort(rules, num_rule, sizeof(rules[0]), [](const void* r1, const void* r2) {
			return strcmp(((const Rule*)r1)->name, ((const Rule*)r2)->name);
		});
	}

	State	parse(Source text)
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
		bool result = top_rule->expression.match_sequence(state, &context);
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
		if (!rule)
			return 0;
		return rule;
	}

	bool	recurse(State& state, Context* parent_context)
	{
		State	start_state(state);	// Save for a failure exit
		state.pc++;	// Skip the '<'

		// Find the end of the rule name in the calling Pegexp:
		const char*	brangle = strchr(state.pc, '>');
		if (!brangle)
			brangle = state.pc+strlen(state.pc);	// Error, no > before end of expression. Survive

		Rule*	sub_rule = lookup(state.pc);
		if (!sub_rule)
		{
#if defined(PEG_TRACE)
			printf("failed to find rule `%.*s`\n", (int)(brangle-state.pc), state.pc);
#endif
			state = start_state;
			return false;
		}

		// Check for left recursion (infinite loop)
		for (Context* pp = parent_context; pp; pp = pp->parent)
		{
			// Is the same Rule already active at the same offset?
			if (pp->rule == sub_rule && pp->text.same(state.text))
			{
#if defined(PEG_TRACE)
				pp->print_path();
				printf(": left recursion detected at `%.10s`\n", state.text.peek());
#endif
				state = start_state;
				return false;
			}
		}

		// The sub_rule commences in the current State, but with the new Pegexp
		State	substate = state;
		substate.pc = sub_rule->expression.pegexp;
		Context	context(this, parent_context, sub_rule, state.text);	// Open a nested Context

#if defined(PEG_TRACE)
		parent_context->print_path();
		printf(": calling %s /%s/ at `%.10s...` (pegexp `%s`)\n", sub_rule->name, substate.pc, state.text.peek(), sub_rule->expression.pegexp);
#endif

		bool	result = sub_rule->expression.match_sequence(substate, &context);

		if (result)
		{
			Source		from = state.text;	// Start of the matched text

			/*
			 * Advance the PC to beyond the sub_rule call (skipping the closing >)
			 * with the text following what we just matched.
			 */
			state.pc = brangle;
			if (*brangle == '>')	// Could be NUL on ill-formed input
				brangle++;
			state = State(brangle, substate.text);

			// Save, if sub_rule is not labelled and the parent wants it
			if (*state.pc != ':'
			 && parent_context->rule->is_saved(sub_rule->name))	// And only if the parent wants it
				(void)context.capture(Result(from, state.text, sub_rule->name, strlen(sub_rule->name)));

#if defined(PEG_TRACE)
			printf("MATCH `%.*s`\n", (int)state.text.bytes_from(from), from.peek());
			printf("continuing at text `%.10s`...\n", state.text.peek());
#endif
		}
#if defined(PEG_TRACE)
		else
			printf("FAIL\n");
#endif
		if (!result)
			state = start_state;
		else
		{
// This is the wrong place to do this:
//			if (parent_context->rule && parent_context->rule->solitary_save())
//				printf("Renamed %s by %s\n", parent_context->rule->saves[0], parent_context->rule->name);
		}
		return result;
	}

protected:
	Rule*		rules;
	int		num_rule;
};
#endif	// PEG_H

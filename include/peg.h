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
template<typename TextPtr, typename Context> class Peg;
template<typename TextPtr, typename Context> class PegPegexp;

/*
 * A PEG parser is made up of a number of named Rules.
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
	typename TextPtr = PegexpDefaultInput,
	int MaxSavesV = 2
>
class PegContextNullCapture
{
public:
	static const int MaxSaves = MaxSavesV;
	using	PegexpT = PegPegexp<TextPtr, PegContextNullCapture>;
	using	Rule = PegRule<PegexpT, MaxSaves>;
	using	PegT = Peg<TextPtr, PegContextNullCapture>;

	PegContextNullCapture(PegT* _peg, PegContextNullCapture* _parent, Rule* _rule, TextPtr _text)
	: peg(_peg)
	, capture_disabled(_parent ? _parent->capture_disabled : 0)
	, parent(_parent)
	, rule(_rule)
	, text(_text)
	{}

	int		capture(PegexpPC name, int name_len, TextPtr from, TextPtr to) { return 0; }
	int		capture_count() const { return 0; }
	void		rollback_capture(int count) {}
	int		capture_disabled;

	PegT*		peg;
	PegContextNullCapture* 	parent;
	Rule*		rule;
	TextPtr		text;
};

// Subclass the Pegexp template to extend virtual functions:
template<
	typename TextPtr = PegexpDefaultInput,
	typename Context = PegContextNullCapture<TextPtr>
> class PegPegexp : public Pegexp<TextPtr, Context>
{
public:
	using	Pegexp = Pegexp<TextPtr, Context>;
	using	State = PegState<TextPtr>;
	PegPegexp(PegexpPC _pegexp_pc) : Pegexp(_pegexp_pc) {}

	virtual State	match_extended(State& state, Context* context)
	{
		if (*state.pc == '<')
			return context->peg->recurse(state, context);
		else
			return Pegexp::match_literal(state);
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
	typename TextPtr = PegexpDefaultInput,
	typename Context = PegContextNullCapture<TextPtr>
>
class Peg
{
public:
	using	Rule = typename Context::Rule;
	using	State = PegState<TextPtr>;
	using	PegexpT = PegPegexp<TextPtr, Context>;

	Peg(Rule* _rules, int _num_rule)
	: rules(_rules), num_rule(_num_rule)
	{
		// We must sort the rules by name, such that a binary search works:
		qsort(rules, num_rule, sizeof(rules[0]), [](const void* r1, const void* r2) {
			return strcmp(((const Rule*)r1)->name, ((const Rule*)r2)->name);
		});
	}

	State	parse(TextPtr text)
	{
		Rule*	top_rule = lookup("TOP");
		assert(top_rule);

#if defined(PEG_TRACE)
		printf(
			"Starting %s at `%.10s...` (pegexp `%s`)\n",
			top_rule->name,
			text.peek(),
			top_rule->expression.code()
		);
#endif

		Context	context(this, 0, top_rule, text);
		State	result = top_rule->expression.match_here(text, &context);
		return result;
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

	State	recurse(State& state, Context* parent_context)
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
			return start_state.fail();
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
				return start_state.fail();
			}
		}

		// The sub_rule commences in the current State, but with the new Pegexp
		State	substate = state;
		substate.pc = sub_rule->expression.code();
		Context	context(this, parent_context, sub_rule, state.text);	// Open a nested Context

#if defined(PEG_TRACE)
		parent_context->print_path();
		printf(": calling %s /%s/ at `%.10s...` (pegexp `%s`)\n", sub_rule->name, substate.pc, state.text.peek(), sub_rule->expression.code());
#endif

		State	result = sub_rule->expression.match_here(substate, &context);

		if (result)
		{
			TextPtr		from = state.text;

			/*
			 * Continue after the sub_rule call (skipping the closing >)
			 * with the text following what we just matched.
			 */
			state.pc = brangle;
			if (*state.pc == '>')	// Could be NUL on ill-formed input
				state.pc++;
			state.text = substate.text;

			// Save, if sub_rule is not labelled and the parent wants it
			if (*state.pc != ':'
			 && parent_context->rule->is_saved(sub_rule->name))	// And only if the parent wants it
				(void)context.capture(sub_rule->name, strlen(sub_rule->name), from, state.text);

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
			state = start_state.fail();
		return state;
	}

protected:
	Rule*		rules;
	int		num_rule;
};
#endif	// PEG_H

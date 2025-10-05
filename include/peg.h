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
template<typename Source, typename Match, typename Context> class Peg;	// The Peg parser

// A Pegexp with the extensions required to be used in a Peg:
template<typename Context> class PegPegexp;

/*
 * A Peg parser is made up of a number of named Rules, each one a Pegexp.
 * An implementation may provide is_captured to cause matches to call Context::capture()
 */
template<
	typename PegexpT
>
class PegRuleNoCapture
{
public:
	PegRuleNoCapture(const char* _name, PegexpT _pegexp)
	: name(_name), expression(_pegexp)
	{}

	const char*	name;
	PegexpT		expression;

	bool		is_captured(const char* label) { return false; }
};

template<
	typename _Source = PegexpDefaultSource
>
class PegDefaultMatch
: public PegexpDefaultMatch<_Source>
{
public:
	using Source = _Source;
	PegDefaultMatch() : PegexpDefaultMatch<Source>() {}	// Default constructor
	PegDefaultMatch(Source from, Source to) : PegexpDefaultMatch<Source>(from, to) {}
};

/*
 * Each time a Rule calls a subordinate Rule, a new Context is created.
 * The Contexts form a linked list back to the topmost rule, for diagnostic and left-recursion purposes.
 * This NoCapture context does no capturing and returns no AST.
 */
template<
	typename _Match = PegDefaultMatch<PegexpDefaultSource>
>
class PegContextNoCapture
{
public:
	using	Match = _Match;
	using	Source = typename Match::Source;
	using	PegexpT = PegPegexp<PegContextNoCapture>;
	using	Rule = PegRuleNoCapture<PegexpT>;
	using	PegT = Peg<Source, Match, PegContextNoCapture>;

	PegContextNoCapture(PegT* _peg, PegContextNoCapture* _parent, Rule* _rule, Source _origin)
	: peg(_peg)
	, capture_disabled(_parent ? _parent->capture_disabled : 0)
	, repetition_nesting(0)
	, parent(_parent)
	, rule(_rule)
	, origin(_origin)
	{}

	// Captures are disabled inside a look-ahead (which can be nested). This holds the nesting count:
	int		capture_disabled;

	// Calls to capture() inside a repetition happen with in_repetition set. This holds the nesting.
	int		repetition_nesting;

	// Every capture gets a capture number, so we can roll it back if needed:
	int		capture_count() const { return 0; }

	// A capture is a named Match. capture() should return the capture_count afterwards.
	int		capture(PegexpPC name, int name_len, Match, bool in_repetition) { return 0; }

	// In some grammars, capture can occur within a failing expression, so we can roll back to a count:
	void		rollback_capture(int count) {}

	// When an atom of a Pegexp fails, the atom (pointer to start and end) and Source location are passed here
	void		record_failure(PegexpPC op, PegexpPC op_end, Source location) {}

	Match		match_result(Source from, Source to)	// Capture the range of text
			{ return Match(from, to); }

	Match		match_failure(Source at)
			{ return Match(at, Source()); }

	PegT*		peg;
	PegContextNoCapture* 	parent;
	Rule*		rule;
	Source		origin;		// Location where this rule started, for detection of left-recursion
};

// Subclass the Pegexp template to extend virtual functions:
template<
	typename Context
> class PegPegexp
: public Pegexp<Context>
{
	using	Rule = typename Context::Rule;
public:
	using	Match = typename Context::Match;
	using	Source = typename Match::Source;
	using	Base = Pegexp<Context>;
	using	State = typename Base::State;
	using	PegT = Peg<Source, Match, Context>;

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

			Match	match = peg->recurse(sub_rule, state, &sub_context);
			if (match.is_failure())
			{
#if defined(PEG_TRACE)
				printf("FAIL\n");
#endif
				state.pc = call_end;	// Allow report_failure to say what call failed
				return false;
			}

#if defined(PEG_TRACE)
			printf("MATCH %s `%.*s`\n", sub_rule->name, (int)state.text.bytes_from(start_state.text), start_state.text.peek());
			printf("continuing /%s/ text `%.10s`...\n", call_end, state.text.peek());
#endif

			int label_size;
			if (label)
			{
				label_size = call_end-label;
				if (call_end[-1] == ':')
					label_size--;
			}
			else
			{		// If the call is not labelled it can still be captured:
				label = sub_rule->name;
				label_size = strlen(label);
			}

			if (context->capture_disabled == 0	// If we're capturing
			 && context->rule->is_captured(label))	// And the parent wants it
			{
				(void)context->capture(
					label, label_size,
					sub_context.match_result(start_state.text, state.text),
					context->repetition_nesting > 0
				);
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
	typename _Source = PegexpDefaultSource,
	typename _Match = PegDefaultMatch<_Source>,
	typename Context = PegContextNoCapture<_Match>
>
class Peg
{
public:
	using	Source = _Source;
	using	Rule = typename Context::Rule;
	using	Match = _Match;
	using	PegexpT = PegPegexp<Context>;
	using	State = typename PegexpT::State;

	static	Rule	rules[];
	static	int	num_rule;

	Peg()
	{
		// We must sort the rules by name, such that a binary search works:
		qsort(rules, num_rule, sizeof(rules[0]), [](const void* r1, const void* r2) {
			return strcmp(((const Rule*)r1)->name, ((const Rule*)r2)->name);
		});
	}

	Match	parse(Source& text)
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

		Context		context(this, 0, top_rule, text);
		Source		start(text);

		return top_rule->expression.match_here(text, &context);
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

	Match	recurse(Rule* sub_rule, State& state, Context* context)
	{
		// Check for left recursion (infinite loop)
		for (Context* pp = context->parent; pp; pp = pp->parent)
		{
			if (pp->origin < state.text)
				break;	// This rule is starting ahead of pp and all its ancestors
			if (pp->rule == sub_rule)
			{
				left_recursion(state);
				return context->match_failure(state.text);
			}
		}

		return sub_rule->expression.match_here(state.text, context);
	}

protected:
	void	left_recursion(State state)
	{
#if defined(PEG_TRACE)
		//pp->print_path();
		printf(": left recursion detected at `%.10s`\n", state.text.peek());
#endif
	}
};
#endif	// PEG_H

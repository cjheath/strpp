#if !defined(PEG_AST_H)
#define PEG_AST_H
/*
 * Px PEG parser generator defined using pegular expression rules
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<peg.h>
#include	<utf8_ptr.h>

#include	<cstdio>

using	PegMemorySourceSup = PegexpPointerSource<GuardedUTF8Ptr>;

using  Source = GuardedUTF8Ptr;
using  PegMemorySourceSup = PegexpPointerSource<GuardedUTF8Ptr>;

// We need some help to extract captured data from PegexpPointerSource:
class PegMemorySource : public PegexpPointerSource<GuardedUTF8Ptr>
{
       using PegMemorySourceSup = PegexpPointerSource<GuardedUTF8Ptr>;
public:
       PegMemorySource() : PegMemorySourceSup() {}
       PegMemorySource(const char* cp) : PegMemorySourceSup(cp) {}
       PegMemorySource(const PegMemorySource& pi) : PegMemorySourceSup(pi) {}

       const UTF8*     peek() const { return data; }
};

class PegFailure {
public:
	PegexpPC	atom;		// Start of the Pegexp literal we failed to match
	int		atom_len;	// Length of the literal (for reporting)
};
// A Failure for each atom tried at furthermost_success location
typedef Array<PegFailure>	PegFailures;

class PegMatch
{
public:
	using Source = PegMemorySource;

	PegMatch()
	{}

	// Capture matched text:
	PegMatch(Source from, Source to)
	{
		if (to.is_null())
			var = Variant();	// type=None -> failure
		else
			var = StrVal(from.peek(), (int)(to - from));
	}

	PegMatch(Variant _var)
	: var(_var)
	{ }

	// Final result constructor with termination point:
	PegMatch(Variant _var, Source reached, PegFailures f)
	: var(_var)
	, furthermost_success(reached)
	, failures(f)
	{ }

	bool		is_failure() const
			{ return var.type() == Variant::None; }

	Variant		var;

	// Furthermost failure and reasons, only populated on return from parse() (outermost Context)
	Source		furthermost_success;
	PegFailures	failures;
};

template<
	typename PegexpT
>
class PegCaptureRule
: public PegRuleNoCapture<PegexpT>
{
public:
	PegCaptureRule(const char* _name, PegexpT _pegexp, const char** _captures)
	: PegRuleNoCapture<PegexpT>(_name, _pegexp)
	, captures(_captures)
	{}

	// Labelled atoms or rules matching these capture names should be returned in the parse match:
	const char**	captures;	// Pointer to zero-terminated array of string pointers.

	bool is_captured(const char* label)	// label maybe not nul-terminated!
	{
		if (!captures)
			return false;
		for (int i = 0; captures[i]; i++)
			if (0 == strncmp(captures[i], label, strlen(captures[i])))
				return true;
		return false;
	}
};

/*
 * PegContext saves:
 * - Captured StrVal and Variant StrVariantMap text from the parse
 * - The location beyond which the parse cannot proceed
 * - What tokens would allow it to get further
 */
class	PegContext
{
public:
	using	Source = PegMemorySource;
	using	Match = PegMatch;
	using	PegT = Peg<Source, Match, PegContext>;
	using	PegexpT = PegPegexp<PegContext>;
	using	Rule = PegCaptureRule<PegexpT>;

	PegContext(PegT* _peg, PegContext* _parent, Rule* _rule, Source _origin)
	: peg(_peg)
	, capture_disabled(_parent ? _parent->capture_disabled : 0)
	, repetition_nesting(0)
	, parent(_parent)
	, rule(_rule)
	, origin(_origin)
	, num_captures(0)
	, furthermost_success(_origin)	// Farthest Source location we reached
	{}

	int		capture(PegexpPC name, int name_len, Match r, bool in_repetition)
	{
		StrVal		key(name, name_len);
		Variant		value(r.var);
		Variant		existing;

		if (value.type() == Variant::String && value.as_strval().length() == 0)
			return num_captures;

		// If this rule captures one item only, collapse the AST:
		if (rule->captures && rule->captures[0] && rule->captures[1] == 0)
		{	// Only one thing is being captured here. Don't nest it, return it.
#if 0
			if (value.type() == Variant::StrVarMap
			 && value.as_variant_map().size() == 1)
			{	// This map has only one entry. Return that
				auto	entry = value.as_variant_map().begin();
				printf(
					"eliding %s in favour of %s\n",
					key.asUTF8(),
					StrVal(entry->first).asUTF8()
				);
				value = entry->second;
				key = entry->first;

#ifdef FLATTEN_ARRAYS
				if (value.type() == Variant::VarArray
				 && value.as_variant_array().length() == 1)
				{
					printf("flattening array\n");
					value = value.as_variant_array()[0];
				}
#endif

			}
#endif
		}

		if (ast.contains(key))
		{		// There are previous captures under this name
			existing = ast[key];
			if (existing.type() != Variant::VarArray)
				existing = Variant(&existing, 1);	// Convert it to an array
			VariantArray va = existing.as_variant_array();
			va += value;		// This Unshares va from the entry stored in the map, so
			ast.put(key, va);	// replace it with this value
			existing = ast[key];
		}
		else	// Insert the match as the first element in an array, or just as itself:
			ast.insert(key, in_repetition ? Variant(&value, 1) : value);

		num_captures++;
		return 0;
	}

	int		capture_count() const
	{
		return num_captures;
	}

	// This grammar should not capture anything that rolls back except to zero, unless on failure:
	void		rollback_capture(int count)
	{
		if (count >= num_captures)
			return;
		if (count == 0)
		{		// We can rollback to zero, but not partway
			ast.clear();
			num_captures = 0;
			return;
		}
		printf("REVISIT: Not rolling back to %d from %d\n", count, num_captures);
	}

	void		record_failure(PegexpPC op, PegexpPC op_end, Source location)
	{
		if (location < furthermost_success)
			return;	// We got further previously

		if (capture_disabled)
			return;	// Failure in lookahead isn't interesting

		// We only need to know about failures of literals, not pegexp operators:
		static	const char*	pegexp_ops = "~@#%_;<`)^$.\\?*+(|&!";
		if (0 != strchr(pegexp_ops, *op))
			return;

		// Record furthermost failure only on the TOP context:
		if (parent)
			return parent->record_failure(op, op_end, location);

		if (furthermost_success < location)
			failures.clear();	// We got further this time, previous failures don't matter

		// Don't double-up failures of the same pegexp against the same text:
		for (int i = 0; i < failures.length(); i++)
			if (failures[i].atom == op)
				return;		// Nothing new here, move along.

		furthermost_success = location;	// We couldn't get past here
		failures.append({op, (int)(op_end-op)});
	}

	Match		match_result(Source from, Source to)
	{
		if (parent == 0)
			return Match(Variant(ast), furthermost_success, failures);
		else if (capture_count() > 0)
			return Match(ast);
		else
			return Match(from, to);
	}
	Match		match_failure(Source at)
	{ return Match(at, Source()); }

#if defined(PEG_TRACE)
	int		depth()
	{ return parent ? parent->depth()+1 : 0; }

	void		print_path(int depth = 0) const
	{
		if (parent)
		{
			parent->print_path(depth+1);
			printf("->");
		}
		else
			printf("@depth=%d: ", depth);
		printf("%s", rule->name);
	}
#endif

	Rule*		rule;		// The Rule this context applies to
	PegT*		peg;		// Place to look up subrules
	PegContext* 	parent;		// Context of the parent (calling) rule
	Source		origin;		// Location where this rule started, for detection of left-recursion

	int		repetition_nesting;	// Number of nested repetition groups, so we know if a capture might be repeated
	int		capture_disabled;	// Counter that bumps up from zero to disable captures

protected:
	int		num_captures;
	StrVariantMap	ast;

	// The next two are populated only on the outermost Context, to be returned from the parse
	Source		furthermost_success;	// Source location of the farthest location the parser reached
	PegFailures	failures;	// A Failure for each atom tried at furthermost_success location
};
#endif // PEG_AST_H

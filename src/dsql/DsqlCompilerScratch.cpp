/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include "../common/common.h"
#include "../dsql/DsqlCompilerScratch.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/jrd.h"
#include "../jrd/blr.h"
#include "../dsql/node.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/errd_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/pass1_proto.h"

using namespace Firebird;
using namespace Dsql;
using namespace Jrd;


// Write out field data type.
// Taking special care to declare international text.
void DsqlCompilerScratch::putDtype(const dsql_fld* field, bool useSubType)
{
#ifdef DEV_BUILD
	// Check if the field describes a known datatype

	if (field->fld_dtype > FB_NELEM(blr_dtypes) || !blr_dtypes[field->fld_dtype])
	{
		SCHAR buffer[100];

		sprintf(buffer, "Invalid dtype %d in BlockNode::putDtype", field->fld_dtype);
		ERRD_bugcheck(buffer);
	}
#endif

	if (field->fld_not_nullable)
		appendUChar(blr_not_nullable);

	if (field->fld_type_of_name.hasData())
	{
		if (field->fld_type_of_table)
		{
			if (field->fld_explicit_collation)
			{
				appendUChar(blr_column_name2);
				appendUChar(field->fld_full_domain ? blr_domain_full : blr_domain_type_of);
				appendMetaString(field->fld_type_of_table->str_data);
				appendMetaString(field->fld_type_of_name.c_str());
				appendUShort(field->fld_ttype);
			}
			else
			{
				appendUChar(blr_column_name);
				appendUChar(field->fld_full_domain ? blr_domain_full : blr_domain_type_of);
				appendMetaString(field->fld_type_of_table->str_data);
				appendMetaString(field->fld_type_of_name.c_str());
			}
		}
		else
		{
			if (field->fld_explicit_collation)
			{
				appendUChar(blr_domain_name2);
				appendUChar(field->fld_full_domain ? blr_domain_full : blr_domain_type_of);
				appendMetaString(field->fld_type_of_name.c_str());
				appendUShort(field->fld_ttype);
			}
			else
			{
				appendUChar(blr_domain_name);
				appendUChar(field->fld_full_domain ? blr_domain_full : blr_domain_type_of);
				appendMetaString(field->fld_type_of_name.c_str());
			}
		}

		return;
	}

	switch (field->fld_dtype)
	{
		case dtype_cstring:
		case dtype_text:
		case dtype_varying:
		case dtype_blob:
			if (!useSubType)
				appendUChar(blr_dtypes[field->fld_dtype]);
			else if (field->fld_dtype == dtype_varying)
			{
				appendUChar(blr_varying2);
				appendUShort(field->fld_ttype);
			}
			else if (field->fld_dtype == dtype_cstring)
			{
				appendUChar(blr_cstring2);
				appendUShort(field->fld_ttype);
			}
			else if (field->fld_dtype == dtype_blob)
			{
				appendUChar(blr_blob2);
				appendUShort(field->fld_sub_type);
				appendUShort(field->fld_ttype);
			}
			else
			{
				appendUChar(blr_text2);
				appendUShort(field->fld_ttype);
			}

			if (field->fld_dtype == dtype_varying)
				appendUShort(field->fld_length - sizeof(USHORT));
			else if (field->fld_dtype != dtype_blob)
				appendUShort(field->fld_length);
			break;

		default:
			appendUChar(blr_dtypes[field->fld_dtype]);
			if (DTYPE_IS_EXACT(field->fld_dtype) || (dtype_quad == field->fld_dtype))
				appendUChar(field->fld_scale);
			break;
	}
}

void DsqlCompilerScratch::putType(const TypeClause& type, bool useSubType)
{
#ifdef DEV_BUILD
	// Check if the field describes a known datatype
	if (type.type > FB_NELEM(blr_dtypes) || !blr_dtypes[type.type])
	{
		SCHAR buffer[100];

		sprintf(buffer, "Invalid dtype %d in put_dtype", type.type);
		ERRD_bugcheck(buffer);
	}
#endif

	if (type.notNull)
		appendUChar(blr_not_nullable);

	if (type.typeOfName.hasData())
	{
		if (type.typeOfTable.hasData())
		{
			if (type.collateSpecified)
			{
				appendUChar(blr_column_name2);
				appendUChar(type.fullDomain ? blr_domain_full : blr_domain_type_of);
				appendMetaString(type.typeOfTable.c_str());
				appendMetaString(type.typeOfName.c_str());
				appendUShort(type.textType);
			}
			else
			{
				appendUChar(blr_column_name);
				appendUChar(type.fullDomain ? blr_domain_full : blr_domain_type_of);
				appendMetaString(type.typeOfTable.c_str());
				appendMetaString(type.typeOfName.c_str());
			}
		}
		else
		{
			if (type.collateSpecified)
			{
				appendUChar(blr_domain_name2);
				appendUChar(type.fullDomain ? blr_domain_full : blr_domain_type_of);
				appendMetaString(type.typeOfName.c_str());
				appendUShort(type.textType);
			}
			else
			{
				appendUChar(blr_domain_name);
				appendUChar(type.fullDomain ? blr_domain_full : blr_domain_type_of);
				appendMetaString(type.typeOfName.c_str());
			}
		}

		return;
	}

	switch (type.type)
	{
		case dtype_cstring:
		case dtype_text:
		case dtype_varying:
		case dtype_blob:
			if (!useSubType)
				appendUChar(blr_dtypes[type.type]);
			else if (type.type == dtype_varying)
			{
				appendUChar(blr_varying2);
				appendUShort(type.textType);
			}
			else if (type.type == dtype_cstring)
			{
				appendUChar(blr_cstring2);
				appendUShort(type.textType);
			}
			else if (type.type == dtype_blob)
			{
				appendUChar(blr_blob2);
				appendUShort(type.subType);
				appendUShort(type.textType);
			}
			else
			{
				appendUChar(blr_text2);
				appendUShort(type.textType);
			}

			if (type.type == dtype_varying)
				appendUShort(type.length - sizeof(USHORT));
			else if (type.type != dtype_blob)
				appendUShort(type.length);
			break;

		default:
			appendUChar(blr_dtypes[type.type]);
			if (DTYPE_IS_EXACT(type.type) || dtype_quad == type.type)
				appendUChar(type.scale);
			break;
	}
}

// Emit dyn for the local variables declared in a procedure or trigger.
void DsqlCompilerScratch::putLocalVariables(const dsql_nod* parameters, SSHORT locals)
{
	if (!parameters)
		return;

	dsql_nod* const* ptr = parameters->nod_arg;

	for (const dsql_nod* const* const end = ptr + parameters->nod_count; ptr < end; ptr++)
	{
		dsql_nod* parameter = *ptr;

		putDebugSrcInfo(parameter->nod_line, parameter->nod_column);

		if (parameter->nod_type == Dsql::nod_def_field)
		{
			dsql_fld* field = (dsql_fld*) parameter->nod_arg[Dsql::e_dfl_field];
			const dsql_nod* const* rest = ptr;

			while (++rest != end)
			{
				if ((*rest)->nod_type == Dsql::nod_def_field)
				{
					const dsql_fld* rest_field = (dsql_fld*) (*rest)->nod_arg[Dsql::e_dfl_field];
					if (field->fld_name == rest_field->fld_name)
					{
						ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-637) <<
								  Arg::Gds(isc_dsql_duplicate_spec) << Arg::Str(field->fld_name));
					}
				}
			}

			dsql_nod* varNode = MAKE_variable(field, field->fld_name.c_str(), VAR_local, 0, 0, locals);
			variables.add(varNode);

			dsql_var* variable = (dsql_var*) varNode->nod_arg[Dsql::e_var_variable];
			putLocalVariable(variable, parameter,
				reinterpret_cast<const dsql_str*>(parameter->nod_arg[Dsql::e_dfl_collate]));

			// Some field attributes are calculated inside
			// putLocalVariable(), so we reinitialize the
			// descriptor
			MAKE_desc_from_field(&varNode->nod_desc, field);

			++locals;
		}
		else if (parameter->nod_type == Dsql::nod_cursor)
		{
			PASS1_statement(this, parameter);
			GEN_statement(this, parameter);
		}
	}
}

// Write out local variable field data type.
void DsqlCompilerScratch::putLocalVariable(dsql_var* variable, dsql_nod* hostParam,
	const dsql_str* collationName)
{
	dsql_fld* field = variable->var_field;

	appendUChar(blr_dcl_variable);
	appendUShort(variable->var_variable_number);
	DDL_resolve_intl_type(this, field, collationName);

	//const USHORT dtype = field->fld_dtype;

	putDtype(field, true);
	//field->fld_dtype = dtype;

	// Check for a default value, borrowed from define_domain
	dsql_nod* node = hostParam ? hostParam->nod_arg[Dsql::e_dfl_default] : NULL;

	if (node || (!field->fld_full_domain && !field->fld_not_nullable))
	{
		appendUChar(blr_assignment);

		if (node)
		{
			fb_assert(node->nod_type == Dsql::nod_def_default);
			PsqlChanger psqlChanger(this, false);
			node = PASS1_node(this, node->nod_arg[Dsql::e_dft_default]);
			GEN_expr(this, node);
		}
		else
			appendUChar(blr_null);	// Initialize variable to NULL

		appendUChar(blr_variable);
		appendUShort(variable->var_variable_number);
	}
	else
	{
		appendUChar(blr_init_variable);
		appendUShort(variable->var_variable_number);
	}

	if (variable->var_name[0])	// Not a function return value
		putDebugVariable(variable->var_variable_number, variable->var_name);

	++hiddenVarsNumber;
}

// Try to resolve variable name against parameters and local variables.
dsql_nod* DsqlCompilerScratch::resolveVariable(const dsql_str* varName)
{
	for (dsql_nod* const* i = variables.begin(); i != variables.end(); ++i)
	{
		dsql_nod* varNode = *i;
		fb_assert(varNode->nod_type == Dsql::nod_variable);

		if (varNode->nod_type == Dsql::nod_variable)
		{
			const dsql_var* variable = (dsql_var*) varNode->nod_arg[Dsql::e_var_variable];
			DEV_BLKCHK(variable, dsql_type_var);

			if (!strcmp(varName->str_data, variable->var_name))
				return varNode;
		}
	}

	return NULL;
}

// Generate BLR for a return.
void DsqlCompilerScratch::genReturn(bool eosFlag)
{
	const bool hasEos = !(flags & (FLAG_TRIGGER | FLAG_FUNCTION));

	if (hasEos && !eosFlag)
		appendUChar(blr_begin);

	appendUChar(blr_send);
	appendUChar(1);
	appendUChar(blr_begin);

	for (Array<dsql_nod*>::const_iterator i = outputVariables.begin(); i != outputVariables.end(); ++i)
	{
		const dsql_nod* parameter = *i;
		const dsql_var* variable = (dsql_var*) parameter->nod_arg[Dsql::e_var_variable];
		appendUChar(blr_assignment);
		appendUChar(blr_variable);
		appendUShort(variable->var_variable_number);
		appendUChar(blr_parameter2);
		appendUChar(variable->var_msg_number);
		appendUShort(variable->var_msg_item);
		appendUShort(variable->var_msg_item + 1);
	}

	if (hasEos)
	{
		appendUChar(blr_assignment);
		appendUChar(blr_literal);
		appendUChar(blr_short);
		appendUChar(0);
		appendUShort((eosFlag ? 0 : 1));
		appendUChar(blr_parameter);
		appendUChar(1);
		appendUShort(USHORT(2 * outputVariables.getCount()));
	}

	appendUChar(blr_end);

	if (hasEos && !eosFlag)
	{
		appendUChar(blr_stall);
		appendUChar(blr_end);
	}
}

void DsqlCompilerScratch::addCTEs(dsql_nod* with)
{
	DEV_BLKCHK(with, dsql_type_nod);
	fb_assert(with->nod_type == Dsql::nod_with);

	if (ctes.getCount())
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // WITH clause can't be nested
				  Arg::Gds(isc_dsql_cte_nested_with));
	}

	if (with->nod_flags & NOD_UNION_RECURSIVE)
		flags |= DsqlCompilerScratch::FLAG_RECURSIVE_CTE;

	const dsql_nod* list = with->nod_arg[0];
	const dsql_nod* const* end = list->nod_arg + list->nod_count;

	for (dsql_nod* const* cte = list->nod_arg; cte < end; cte++)
	{
		fb_assert((*cte)->nod_type == Dsql::nod_derived_table);

		if (with->nod_flags & NOD_UNION_RECURSIVE)
		{
			currCtes.push(*cte);
			PsqlChanger changer(this, false);
			ctes.add(pass1RecursiveCte(*cte));
			currCtes.pop();

			// Add CTE name into CTE aliases stack. It allows later to search for
			// aliases of given CTE.
			const dsql_str* cteName = (dsql_str*) (*cte)->nod_arg[Dsql::e_derived_table_alias];
			addCTEAlias(cteName);
		}
		else
			ctes.add(*cte);
	}
}

dsql_nod* DsqlCompilerScratch::findCTE(const dsql_str* name)
{
	for (size_t i = 0; i < ctes.getCount(); ++i)
	{
		dsql_nod* cte = ctes[i];
		const dsql_str* cteName = (dsql_str*) cte->nod_arg[Dsql::e_derived_table_alias];

		if (name->str_length == cteName->str_length &&
			strncmp(name->str_data, cteName->str_data, cteName->str_length) == 0)
		{
			return cte;
		}
	}

	return NULL;
}

void DsqlCompilerScratch::clearCTEs()
{
	flags &= ~DsqlCompilerScratch::FLAG_RECURSIVE_CTE;
	ctes.clear();
	cteAliases.clear();
}

void DsqlCompilerScratch::checkUnusedCTEs() const
{
	for (size_t i = 0; i < ctes.getCount(); ++i)
	{
		const dsql_nod* cte = ctes[i];

		if (!(cte->nod_flags & NOD_DT_CTE_USED))
		{
			const dsql_str* cteName = (dsql_str*) cte->nod_arg[Dsql::e_derived_table_alias];

			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					  Arg::Gds(isc_dsql_cte_not_used) << Arg::Str(cteName->str_data));
		}
	}
}

// Process derived table which can be recursive CTE.
// If it is non-recursive return input node unchanged.
// If it is recursive return new derived table which is an union of union of anchor (non-recursive)
// queries and union of recursive queries. Check recursive queries to satisfy various criterias.
// Note that our parser is right-to-left therefore nested list linked as first node in parent list
// and second node is always query spec.
//  For example, if we have 4 CTE's where first two is non-recursive and last two is recursive:
//
//				list							  union
//			  [0]	[1]						   [0]		[1]
//			list	cte3		===>		anchor		recursive
//		  [0]	[1]						 [0]	[1]		[0]		[1]
//		list	cte3					cte1	cte2	cte3	cte4
//	  [0]	[1]
//	cte1	cte2
//
// Also, we should not change layout of original parse tree to allow it to be parsed again if
// needed. Therefore recursive part is built using newly allocated list nodes.
dsql_nod* DsqlCompilerScratch::pass1RecursiveCte(dsql_nod* input)
{
	dsql_str* const cte_alias = (dsql_str*) input->nod_arg[Dsql::e_derived_table_alias];
	dsql_nod* const select_expr = input->nod_arg[Dsql::e_derived_table_rse];
	dsql_nod* query = select_expr->nod_arg[Dsql::e_sel_query_spec];

	if (query->nod_type != Dsql::nod_list && pass1RseIsRecursive(query))
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Recursive CTE (%s) must be an UNION
				  Arg::Gds(isc_dsql_cte_not_a_union) << Arg::Str(cte_alias->str_data));
	}

	// split queries list on two parts: anchor and recursive
	dsql_nod* anchorRse = NULL, *recursiveRse = NULL;
	dsql_nod* qry = query;

	dsql_nod* newQry = MAKE_node(Dsql::nod_list, 2);
	newQry->nod_flags = query->nod_flags;

	while (true)
	{
		dsql_nod* rse = NULL;

		if (qry->nod_type == Dsql::nod_list)
			rse = qry->nod_arg[1];
		else
			rse = qry;

		dsql_nod* newRse = pass1RseIsRecursive(rse);

		if (newRse) // rse is recursive
		{
			if (anchorRse)
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// CTE '%s' defined non-recursive member after recursive
					Arg::Gds(isc_dsql_cte_nonrecurs_after_recurs) << Arg::Str(cte_alias->str_data));
			}

			if (newRse->nod_arg[Dsql::e_qry_distinct])
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// Recursive member of CTE '%s' has %s clause
					Arg::Gds(isc_dsql_cte_wrong_clause) << Arg::Str(cte_alias->str_data) <<
														   Arg::Str("DISTINCT"));
			}

			if (newRse->nod_arg[Dsql::e_qry_group])
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// Recursive member of CTE '%s' has %s clause
					Arg::Gds(isc_dsql_cte_wrong_clause) << Arg::Str(cte_alias->str_data) <<
														   Arg::Str("GROUP BY"));
			}

			if (newRse->nod_arg[Dsql::e_qry_having])
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// Recursive member of CTE '%s' has %s clause
					Arg::Gds(isc_dsql_cte_wrong_clause) << Arg::Str(cte_alias->str_data) <<
														   Arg::Str("HAVING"));
			}
			// hvlad: we need also forbid any aggregate function here
			// but for now i have no idea how to do it simple

			if ((newQry->nod_type == Dsql::nod_list) && !(newQry->nod_flags & NOD_UNION_ALL))
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
					// Recursive members of CTE (%s) must be linked with another members via UNION ALL
					Arg::Gds(isc_dsql_cte_union_all) << Arg::Str(cte_alias->str_data));
			}

			if (!recursiveRse)
				recursiveRse = newQry;

			newRse->nod_flags |= NOD_SELECT_EXPR_RECURSIVE;

			if (qry->nod_type == Dsql::nod_list)
				newQry->nod_arg[1] = newRse;
			else
				newQry->nod_arg[0] = newRse;
		}
		else
		{
			if (qry->nod_type == Dsql::nod_list)
				newQry->nod_arg[1] = rse;
			else
				newQry->nod_arg[0] = rse;

			if (!anchorRse)
			{
				if (qry->nod_type == Dsql::nod_list)
					anchorRse = newQry;
				else
					anchorRse = rse;
			}
		}

		if (qry->nod_type != Dsql::nod_list)
			break;

		qry = qry->nod_arg[0];

		if (qry->nod_type == Dsql::nod_list)
		{
			newQry->nod_arg[0] = MAKE_node(Dsql::nod_list, 2);
			newQry = newQry->nod_arg[0];
			newQry->nod_flags = qry->nod_flags;
		}
	}

	if (!recursiveRse)
		return input;

	if (!anchorRse)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
			// Non-recursive member is missing in CTE '%s'
			Arg::Gds(isc_dsql_cte_miss_nonrecursive) << Arg::Str(cte_alias->str_data));
	}

	qry = recursiveRse;
	dsql_nod* list = NULL;

	while (qry->nod_arg[0] != anchorRse)
	{
		list = qry;
		qry = qry->nod_arg[0];
	}

	qry->nod_arg[0] = 0;

	if (list)
		list->nod_arg[0] = qry->nod_arg[1];
	else
		recursiveRse = qry->nod_arg[1];

	dsql_nod* unionNode = MAKE_node(Dsql::nod_list, 2);
	unionNode->nod_flags = NOD_UNION_ALL | NOD_UNION_RECURSIVE;
	unionNode->nod_arg[0] = anchorRse;
	unionNode->nod_arg[1] = recursiveRse;

	dsql_nod* select = MAKE_node(Dsql::nod_select_expr, Dsql::e_sel_count);
	select->nod_arg[Dsql::e_sel_query_spec] = unionNode;
	select->nod_arg[Dsql::e_sel_order] = select->nod_arg[Dsql::e_sel_rows] =
		select->nod_arg[Dsql::e_sel_with_list] = NULL;

	dsql_nod* node = MAKE_node(Dsql::nod_derived_table, Dsql::e_derived_table_count);
	dsql_str* alias = (dsql_str*) input->nod_arg[Dsql::e_derived_table_alias];
	node->nod_arg[Dsql::e_derived_table_alias] = (dsql_nod*) alias;
	node->nod_arg[Dsql::e_derived_table_column_alias] =
		input->nod_arg[Dsql::e_derived_table_column_alias];
	node->nod_arg[Dsql::e_derived_table_rse] = select;
	node->nod_arg[Dsql::e_derived_table_context] = input->nod_arg[Dsql::e_derived_table_context];

	return node;
}

// Check if rse is recursive. If recursive reference is a table in the FROM list remove it.
// If recursive reference is a part of join add join boolean (returned by pass1JoinIsRecursive)
// to the WHERE clause. Punt if more than one recursive reference is found.
dsql_nod* DsqlCompilerScratch::pass1RseIsRecursive(dsql_nod* input)
{
	fb_assert(input->nod_type == Dsql::nod_query_spec);

	dsql_nod* result = MAKE_node(Dsql::nod_query_spec, Dsql::e_qry_count);
	memcpy(result->nod_arg, input->nod_arg, Dsql::e_qry_count * sizeof(dsql_nod*));

	dsql_nod* srcTables = input->nod_arg[Dsql::e_qry_from];
	dsql_nod* dstTables = MAKE_node(Dsql::nod_list, srcTables->nod_count);
	result->nod_arg[Dsql::e_qry_from] = dstTables;

	dsql_nod** pDstTable = dstTables->nod_arg;
	dsql_nod** pSrcTable = srcTables->nod_arg;
	dsql_nod** end = srcTables->nod_arg + srcTables->nod_count;
	bool found = false;

	for (dsql_nod** prev = pDstTable; pSrcTable < end; ++pSrcTable, ++pDstTable)
	{
		*prev++ = *pDstTable = *pSrcTable;

		switch ((*pDstTable)->nod_type)
		{
			case Dsql::nod_rel_proc_name:
			case Dsql::nod_relation_name:
				if (pass1RelProcIsRecursive(*pDstTable))
				{
					if (found)
					{
						ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
								  // Recursive member of CTE can't reference itself more than once
								  Arg::Gds(isc_dsql_cte_mult_references));
					}
					found = true;

					prev--;
					dstTables->nod_count--;
				}
				break;

			case Dsql::nod_join:
			{
				*pDstTable = MAKE_node(Dsql::nod_join, Dsql::e_join_count);
				memcpy((*pDstTable)->nod_arg, (*pSrcTable)->nod_arg,
					Dsql::e_join_count * sizeof(dsql_nod*));

				dsql_nod* joinBool = pass1JoinIsRecursive(*pDstTable);

				if (joinBool)
				{
					if (found)
					{
						ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
								  // Recursive member of CTE can't reference itself more than once
								  Arg::Gds(isc_dsql_cte_mult_references));
					}

					found = true;

					result->nod_arg[Dsql::e_qry_where] = PASS1_compose(
						result->nod_arg[Dsql::e_qry_where], joinBool, blr_and);
				}

				break;
			}

			case Dsql::nod_derived_table:
				break;

			default:
				fb_assert(false);
		}
	}

	return found ? result : NULL;
}

// Check if table reference is recursive i.e. its name is equal to the name of current processing CTE.
bool DsqlCompilerScratch::pass1RelProcIsRecursive(dsql_nod* input)
{
	const dsql_str* relName = NULL;
	const dsql_str* relAlias = NULL;

	switch (input->nod_type)
	{
		case Dsql::nod_rel_proc_name:
			relName = (dsql_str*) input->nod_arg[Dsql::e_rpn_name];
			relAlias = (dsql_str*) input->nod_arg[Dsql::e_rpn_alias];
			break;

		case Dsql::nod_relation_name:
			relName = (dsql_str*) input->nod_arg[Dsql::e_rln_name];
			relAlias = (dsql_str*) input->nod_arg[Dsql::e_rln_alias];
			break;

		default:
			return false;
	}

	fb_assert(currCtes.hasData());
	const dsql_nod* curr_cte = currCtes.object();
	const dsql_str* cte_name = (dsql_str*) curr_cte->nod_arg[Dsql::e_derived_table_alias];

	const bool recursive = (cte_name->str_length == relName->str_length) &&
		(strncmp(relName->str_data, cte_name->str_data, cte_name->str_length) == 0);

	if (recursive)
		addCTEAlias(relAlias ? relAlias : relName);

	return recursive;
}

// Check if join have recursive members. If found remove this member from join and return its
// boolean (to be added into WHERE clause).
// We must remove member only if it is a table reference. Punt if recursive reference is found in
// outer join or more than one recursive reference is found
dsql_nod* DsqlCompilerScratch::pass1JoinIsRecursive(dsql_nod*& input)
{
	const NOD_TYPE join_type = input->nod_arg[Dsql::e_join_type]->nod_type;
	bool remove = false;

	bool leftRecursive = false;
	dsql_nod* leftBool = NULL;
	dsql_nod** join_table = &input->nod_arg[Dsql::e_join_left_rel];

	if ((*join_table)->nod_type == Dsql::nod_join)
	{
		leftBool = pass1JoinIsRecursive(*join_table);
		leftRecursive = (leftBool != NULL);
	}
	else
	{
		leftBool = input->nod_arg[Dsql::e_join_boolean];
		leftRecursive = pass1RelProcIsRecursive(*join_table);

		if (leftRecursive)
			remove = true;
	}

	if (leftRecursive && join_type != Dsql::nod_join_inner)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Recursive member of CTE can't be member of an outer join
				  Arg::Gds(isc_dsql_cte_outer_join));
	}

	bool rightRecursive = false;
	dsql_nod* rightBool = NULL;

	join_table = &input->nod_arg[Dsql::e_join_rght_rel];

	if ((*join_table)->nod_type == Dsql::nod_join)
	{
		rightBool = pass1JoinIsRecursive(*join_table);
		rightRecursive = (rightBool != NULL);
	}
	else
	{
		rightBool = input->nod_arg[Dsql::e_join_boolean];
		rightRecursive = pass1RelProcIsRecursive(*join_table);

		if (rightRecursive)
			remove = true;
	}

	if (rightRecursive && join_type != Dsql::nod_join_inner)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Recursive member of CTE can't be member of an outer join
				  Arg::Gds(isc_dsql_cte_outer_join));
	}

	if (leftRecursive && rightRecursive)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Recursive member of CTE can't reference itself more than once
				  Arg::Gds(isc_dsql_cte_mult_references));
	}

	if (leftRecursive)
	{
		if (remove)
			input = input->nod_arg[Dsql::e_join_rght_rel];

		return leftBool;
	}

	if (rightRecursive)
	{
		if (remove)
			input = input->nod_arg[Dsql::e_join_left_rel];

		return rightBool;
	}

	return NULL;
}

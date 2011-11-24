/*
 *	PROGRAM:	Dynamic SQL runtime support
 *	MODULE:		ddl.cpp
 *	DESCRIPTION:	Utilities for generating ddl
 *
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
 *
 * 2001.5.20 Claudio Valderrama: Stop null pointer that leads to a crash,
 * caused by incomplete yacc syntax that allows ALTER DOMAIN dom SET;
 *
 * 2001.07.06 Sean Leyne - Code Cleanup, removed "#ifdef READONLY_DATABASE"
 *                         conditionals, as the engine now fully supports
 *                         readonly databases.
 * December 2001 Mike Nordell - Attempt to make it C++
 *
 * 2001.5.20 Claudio Valderrama: Stop null pointer that leads to a crash,
 * caused by incomplete yacc syntax that allows ALTER DOMAIN dom SET;
 * 2001.5.29 Claudio Valderrama: Check for view v/s relation in DROP
 * command will stop a user that uses DROP VIEW and drops a table by
 * accident and vice-versa.
 * 2001.5.30 Claudio Valderrama: alter column should use 1..N for the
 * position argument since the call comes from SQL DDL.
 * 2001.6.27 Claudio Valderrama: DDL_resolve_intl_type() was adding 2 to the
 * length of varchars instead of just checking that len+2<=MAX_COLUMN_SIZE.
 * It required a minor change to put_field() where it was decremented, too.
 * 2001.6.27 Claudio Valderrama: Finally stop users from invoking the same option
 * several times when altering a domain. Specially dangerous with text data types.
 * Ex: alter domain d type char(5) type varchar(5) default 'x' default 'y';
 * Bear in mind that if DYN functions are addressed directly, this protection
 * becomes a moot point.
 * 2001.6.30 Claudio Valderrama: revert changes from 2001.6.26 because the code
 * is called from several places and there are more functions, even in metd.c,
 * playing the same nonsense game with the field's length, so it needs more
 * careful examination. For now, the new checks in DYN_MOD should catch most anomalies.
 * 2001.7.3 Claudio Valderrama: fix Firebird Bug #223059 with mismatch between number
 * of declared fields for a VIEW and effective fields in the SELECT statement.
 * 2001.07.22 Claudio Valderrama: minor fixes and improvements.
 * 2001.08.18 Claudio Valderrama: RECREATE PROCEDURE.
 * 2001.10.01 Claudio Valderrama: modify_privilege() should recognize that a ROLE can
 *   now be made an explicit grantee.
 * 2001.10.08 Claudio Valderrama: implement fb_sysflag enum values for autogenerated
 *   non-system triggers so DFW can recognize them easily.
 * 2001.10.26 Claudio Valderrama: added a call to the new METD_drop_function()
 *   in DDL_execute() so the metadata cache for udfs can be refreshed.
 * 2001.12.06 Claudio Valderrama: DDL_resolve_intl_type should calculate field length
 * 2002.08.04 Claudio Valderrama: allow declaring and defining variables at the same time
 * 2002.08.04 Dmitry Yemanov: ALTER VIEW
 * 2002.08.31 Dmitry Yemanov: allowed user-defined index names for PK/FK/UK constraints
 * 2002.09.01 Dmitry Yemanov: RECREATE VIEW
 * 2002.09.12 Nickolay Samofatov: fixed cached metadata errors
 * 2004.01.16 Vlad Horsun: added support for default parameters and
 *   EXECUTE BLOCK statement
 * Adriano dos Santos Fernandes
 */

#include "firebird.h"
#include "dyn_consts.h"
#include <stdio.h>
#include <string.h>
#include "../jrd/SysFunction.h"
#include "../common/classes/MetaName.h"
#include "../dsql/dsql.h"
#include "../dsql/node.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/ibase.h"
#include "../jrd/Attachment.h"
#include "../jrd/RecordSourceNodes.h"
#include "../jrd/intl.h"
#include "../jrd/intl_classes.h"
#include "../jrd/jrd.h"
#include "../jrd/flags.h"
#include "../jrd/constants.h"
#include "../dsql/errd_proto.h"
#include "../dsql/ddl_proto.h"
#include "../dsql/gen_proto.h"
#include "../dsql/make_proto.h"
#include "../dsql/metd_proto.h"
#include "../dsql/pass1_proto.h"
#include "../dsql/utld_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/dyn_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/thread_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/vio_proto.h"
#include "../yvalve/why_proto.h"
#include "../common/utils_proto.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/DSqlDataTypeUtil.h"
#include "../common/StatusArg.h"

#ifdef DSQL_DEBUG
#include "../common/prett_proto.h"
#endif

using namespace Jrd;
using namespace Dsql;
using namespace Firebird;


static void assign_field_length(dsql_fld*, USHORT);
static void define_computed(DsqlCompilerScratch*, dsql_nod*, dsql_fld*, dsql_nod*);
static void define_filter(DsqlCompilerScratch*);
static SSHORT getBlobFilterSubType(DsqlCompilerScratch* dsqlScratch, const dsql_nod* node);
static void define_index(DsqlCompilerScratch*);
static void generate_dyn(DsqlCompilerScratch*, dsql_nod*);
static void grant_revoke(DsqlCompilerScratch*);
static void modify_privilege(DsqlCompilerScratch* dsqlScratch, NOD_TYPE type, SSHORT option,
							 const UCHAR* privs, const dsql_nod* table,
							 const dsql_nod* user, const dsql_nod* grantor,
							 const dsql_str* field_name);
static char modify_privileges(DsqlCompilerScratch*, NOD_TYPE, SSHORT, const dsql_nod*,
	const dsql_nod*, const dsql_nod*, const dsql_nod*);
static void process_role_nm_list(DsqlCompilerScratch*, SSHORT, const dsql_nod*, const dsql_nod*, NOD_TYPE, const dsql_nod*);
static void define_user(DsqlCompilerScratch*, UCHAR);
static void put_grantor(DsqlCompilerScratch* dsqlScratch, const dsql_nod* grantor);
static void post_607(const Arg::StatusVector& v);
static void put_user_grant(DsqlCompilerScratch* dsqlScratch, const dsql_nod* user);


const int DEFAULT_BLOB_SEGMENT_SIZE = 80; // bytes


void DDL_execute(dsql_req* request)
{
/**************************************
 *
 *	D D L _ e x e c u t e
 *
 **************************************
 *
 * Functional description
 *	Call access method layered service DYN
 *	to interpret dyn string and perform
 *	metadata updates.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	const DsqlCompiledStatement* statement = request->getStatement();

#ifdef DSQL_DEBUG
	if (DSQL_debug & 4)
	{
		dsql_trace("Output DYN string for DDL:");
		PRETTY_print_dyn(statement->getDdlData().begin(), gds__trace_printer, NULL, 0);
	}
#endif

	const NOD_TYPE type = statement->getDdlNode()->nod_type;

	if (type == nod_class_stmtnode)
	{
		fb_utils::init_status(tdbb->tdbb_status_vector);	// Do the same as DYN_ddl does.

		// run all statements under savepoint control
		{	// scope
			AutoSavePoint savePoint(tdbb, request->req_transaction);

			DdlNode* ddlNode = reinterpret_cast<DdlNode*>(statement->getDdlNode()->nod_arg[0]);
			ddlNode->executeDdl(tdbb, statement->getDdlScratch(), request->req_transaction);

			savePoint.release();	// everything is ok
		}
	}
	else
	{
		fb_assert(statement->getDdlData().getCount() <= MAX_ULONG);
		DYN_ddl(request->req_transaction,
				statement->getDdlData().getCount(), statement->getDdlData().begin(),
				*statement->getSqlText());
	}

	JRD_autocommit_ddl(tdbb, request->req_transaction);
}


void DDL_generate(DsqlCompilerScratch* dsqlScratch, dsql_nod* node)
{
/**************************************
 *
 *	D D L _ g e n e r a t e
 *
 **************************************
 *
 * Functional description
 *	Generate the DYN string for a
 *	metadata update.  Done during the
 *	prepare phase.
 *
 **************************************/

	if (dsqlScratch->getAttachment()->dbb_read_only)
	{
		ERRD_post(Arg::Gds(isc_read_only_database));
		return;
	}

	dsqlScratch->getStatement()->setDdlNode(node);

	if (node->nod_type != nod_class_stmtnode)
		dsqlScratch->appendUChar(isc_dyn_version_1);

	generate_dyn(dsqlScratch, node);

	if (node->nod_type != nod_class_stmtnode)
	{
		dsqlScratch->appendUChar(isc_dyn_eoc);

		// Store DYN data in the statement.
		dsqlScratch->getStatement()->setDdlData(dsqlScratch->getBlrData());
	}
}


//
// Determine whether ids or names should be referenced
// when generating blr for fields and relations.
//
bool DDL_ids(const DsqlCompilerScratch* scratch)
{
	return !scratch->getStatement()->getDdlNode();
}


//
// See the next function for description. This is only a
// wrapper that sets the last parameter to false to indicate
// we are creating a field, not modifying one.
//
void DDL_resolve_intl_type(DsqlCompilerScratch* dsqlScratch, dsql_fld* field,
	const MetaName& collation_name)
{
	DDL_resolve_intl_type2(dsqlScratch, field, collation_name, false);
}


void DDL_resolve_intl_type2(DsqlCompilerScratch* dsqlScratch, dsql_fld* field,
	const MetaName& collation_name, bool modifying)
{
/**************************************
 *
 *  D D L _ r e s o l v e _ i n t l _ t y p e 2
 *
 **************************************
 *
 * Function

 *	If the field is defined with a character set or collation,
 *	resolve the names to a subtype now.
 *
 *	Also resolve the field length & whatnot.
 *
 *  If the field is being created, it will pick the db-wide charset
 *  and collation if not specified. If the field is being modified,
 *  since we don't allow changes to those attributes, we'll go and
 *  calculate the correct old lenth from the field itself so DYN
 *  can validate the change properly.
 *
 *	For International text fields, this is a good time to calculate
 *	their actual size - when declared they were declared in
 *	lengths of CHARACTERs, not BYTES.
 *
 **************************************/

	if (field->fld_type_of_name.hasData())
	{
		if (field->fld_type_of_table.hasData())
		{
			dsql_rel* relation = METD_get_relation(dsqlScratch->getTransaction(), dsqlScratch,
				field->fld_type_of_table.c_str());
			const dsql_fld* fld = NULL;

			if (relation)
			{
				const MetaName fieldName(field->fld_type_of_name);

				for (fld = relation->rel_fields; fld; fld = fld->fld_next)
				{
					if (fieldName == fld->fld_name)
					{
						field->fld_dimensions = fld->fld_dimensions;
						field->fld_source = fld->fld_source;
						field->fld_length = fld->fld_length;
						field->fld_scale = fld->fld_scale;
						field->fld_sub_type = fld->fld_sub_type;
						field->fld_character_set_id = fld->fld_character_set_id;
						field->fld_collation_id = fld->fld_collation_id;
						field->fld_character_length = fld->fld_character_length;
						field->fld_flags = fld->fld_flags;
						field->fld_dtype = fld->fld_dtype;
						field->fld_seg_length = fld->fld_seg_length;
						break;
					}
				}
			}

			if (!fld)
			{
				// column @1 does not exist in table/view @2
				post_607(Arg::Gds(isc_dyn_column_does_not_exist) <<
						 		Arg::Str(field->fld_type_of_name) <<
								field->fld_type_of_table);
			}
		}
		else
		{
			if (!METD_get_domain(dsqlScratch->getTransaction(), field, field->fld_type_of_name.c_str()))
			{
				// Specified domain or source field does not exist
				post_607(Arg::Gds(isc_dsql_domain_not_found) << Arg::Str(field->fld_type_of_name));
			}
		}

		if (field->fld_dimensions != 0)
		{
			ERRD_post(Arg::Gds(isc_wish_list) <<
				Arg::Gds(isc_random) <<
				Arg::Str("Usage of domain or TYPE OF COLUMN of array type in PSQL"));
		}
	}

	if ((field->fld_dtype > dtype_any_text) && field->fld_dtype != dtype_blob)
	{
		if (field->fld_character_set || collation_name.hasData() || field->fld_flags & FLD_national)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) << Arg::Gds(isc_collation_requires_text));
		}
		return;
	}

	if (field->fld_dtype == dtype_blob)
	{
		if (field->fld_sub_type_name)
		{
			SSHORT blob_sub_type;
			if (!METD_get_type(dsqlScratch->getTransaction(),
					reinterpret_cast<const dsql_str*>(field->fld_sub_type_name),
					"RDB$FIELD_SUB_TYPE", &blob_sub_type))
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
						  Arg::Gds(isc_dsql_datatype_err) <<
						  Arg::Gds(isc_dsql_blob_type_unknown) <<
						  		Arg::Str(((dsql_str*) field->fld_sub_type_name)->str_data));
			}
			field->fld_sub_type = blob_sub_type;
		}

		if (field->fld_sub_type > isc_blob_text)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
					  Arg::Gds(isc_subtype_for_internal_use));
		}

		if (field->fld_character_set && (field->fld_sub_type == isc_blob_untyped))
			field->fld_sub_type = isc_blob_text;

		if (field->fld_character_set && (field->fld_sub_type != isc_blob_text))
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_collation_requires_text));
		}

		if (collation_name.hasData() && (field->fld_sub_type != isc_blob_text))
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_collation_requires_text));
		}

		if (field->fld_sub_type != isc_blob_text)
			return;
	}

	if (field->fld_character_set_id != 0 && collation_name.isEmpty())
	{
		// This field has already been resolved once, and the collation
		// hasn't changed.  Therefore, no need to do it again.
		return;
	}


	if (modifying)
	{
#ifdef DEV_BUILD
		const dsql_rel* relation = dsqlScratch->relation;
#endif
		const dsql_fld* afield = field->fld_next;
		USHORT bpc = 0;

		while (afield)
		{
			// The first test is redundant.
			if (afield != field && afield->fld_relation && afield->fld_name == field->fld_name)
			{
				fb_assert(afield->fld_relation == relation || !relation);
				break;
			}

			afield = afield->fld_next;
		}

		if (afield)
		{
			field->fld_character_set_id = afield->fld_character_set_id;
			bpc = METD_get_charset_bpc(dsqlScratch->getTransaction(), field->fld_character_set_id);
			field->fld_collation_id = afield->fld_collation_id;
			field->fld_ttype = afield->fld_ttype;

			if (afield->fld_flags & FLD_national)
				field->fld_flags |= FLD_national;
			else
				field->fld_flags &= ~FLD_national;

			assign_field_length (field, bpc);
			return;
		}
	}

	if (!(field->fld_character_set || field->fld_character_set_id ||	// set if a domain
		(field->fld_flags & FLD_national)))
	{
		// Attach the database default character set, if not otherwise specified

		const dsql_str* dfl_charset = NULL;

		if (dsqlScratch->getStatement()->getDdlNode() ||
			(dsqlScratch->flags & (
				DsqlCompilerScratch::FLAG_FUNCTION | DsqlCompilerScratch::FLAG_PROCEDURE |
				DsqlCompilerScratch::FLAG_TRIGGER)))
		{
			dfl_charset = METD_get_default_charset(dsqlScratch->getTransaction());
		}
		else
		{
			USHORT charSet = dsqlScratch->getAttachment()->dbb_attachment->att_charset;
			if (charSet != CS_NONE)
			{
				MetaName charSetName = METD_get_charset_name(dsqlScratch->getTransaction(), charSet);
				dfl_charset = MAKE_string(charSetName.c_str(), charSetName.length());
			}
		}

		if (dfl_charset)
			field->fld_character_set = (dsql_nod*) dfl_charset;
		else
		{
			// If field is not specified with NATIONAL, or CHARACTER SET
			// treat it as a single-byte-per-character field of character set NONE.
			assign_field_length(field, 1);
			field->fld_ttype = 0;

			if (collation_name.isEmpty())
				return;
		}
	}

	const char* charset_name = NULL;

	if (field->fld_flags & FLD_national)
		charset_name = NATIONAL_CHARACTER_SET;
	else if (field->fld_character_set)
		charset_name = ((dsql_str*) field->fld_character_set)->str_data;

	// Find an intlsym for any specified character set name & collation name
	const dsql_intlsym* resolved_type = NULL;

	if (charset_name)
	{
		const dsql_intlsym* resolved_charset =
			METD_get_charset(dsqlScratch->getTransaction(), (USHORT) strlen(charset_name), charset_name);

		// Error code -204 (IBM's DB2 manual) is close enough
		if (!resolved_charset)
		{
			// specified character set not found
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_charset_not_found) << Arg::Str(charset_name));
		}

		field->fld_character_set_id = resolved_charset->intlsym_charset_id;
		resolved_type = resolved_charset;
	}

	if (collation_name.hasData())
	{
		const dsql_intlsym* resolved_collation = METD_get_collation(dsqlScratch->getTransaction(),
			collation_name, field->fld_character_set_id);

		if (!resolved_collation)
		{
			MetaName charSetName;

			if (charset_name)
				charSetName = charset_name;
			else
			{
				charSetName = METD_get_charset_name(dsqlScratch->getTransaction(),
					field->fld_character_set_id);
			}

			// Specified collation not found
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_collation_not_found) << collation_name << charSetName);
		}

		// If both specified, must be for same character set
		// A "literal constant" must be handled (charset as ttype_dynamic)

		resolved_type = resolved_collation;

		if ((field->fld_character_set_id != resolved_type->intlsym_charset_id) &&
			(field->fld_character_set_id != ttype_dynamic))
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_collation_not_for_charset) << collation_name);
		}

		field->fld_explicit_collation = true;
	}

	assign_field_length (field, resolved_type->intlsym_bytes_per_char);

	field->fld_ttype = resolved_type->intlsym_ttype;
	field->fld_character_set_id = resolved_type->intlsym_charset_id;
	field->fld_collation_id = resolved_type->intlsym_collate_id;
}


static void assign_field_length(dsql_fld* field, USHORT bytes_per_char)
{
/**************************************
 *
 *  a s s i g n _ f i e l d _ l e n g t h
 *
 **************************************
 *
 * Function
 *  We'll see if the field's length fits in the maximum
 *  allowed field, including charset and space for varchars.
 *  Either we raise an error or assign the field's length.
 *  If the charlen comes as zero, we do nothing, although we
 *  know that DYN, MET and DFW will blindly set field length
 *  to zero if they don't catch charlen or another condition.
 *
 **************************************/

	if (field->fld_character_length)
	{
		ULONG field_length = (ULONG) bytes_per_char * field->fld_character_length;

		if (field->fld_dtype == dtype_varying) {
			field_length += sizeof(USHORT);
		}
		if (field_length > MAX_COLUMN_SIZE)
		{
			ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
					  Arg::Gds(isc_dsql_datatype_err) <<
                      Arg::Gds(isc_imp_exc) <<
					  Arg::Gds(isc_field_name) << Arg::Str(field->fld_name));
		}
		field->fld_length = (USHORT) field_length;
	}

}


static void define_computed(DsqlCompilerScratch* dsqlScratch,
							dsql_nod* relation_node,
							dsql_fld* field,
							dsql_nod* node)
{
/**************************************
 *
 *	d e f i n e _ c o m p u t e d
 *
 **************************************
 *
 * Function
 *	Create the ddl to define a computed field
 *	or an expression index.
 *
 **************************************/

	DsqlCompiledStatement* statement = dsqlScratch->getStatement();
	dsql_nod* const saved_ddl_node = statement->getDdlNode();
	statement->setDdlNode(node);

	// Get the table node & set up correct context
	DDL_reset_context_stack(dsqlScratch);

	dsc save_desc;
	// Save the size of the field if it is specified
	save_desc.dsc_dtype = 0;

	if (field && field->fld_dtype)
	{
		fb_assert(field->fld_dtype <= MAX_UCHAR);
		save_desc.dsc_dtype = (UCHAR) field->fld_dtype;
		save_desc.dsc_length = field->fld_length;
		fb_assert(field->fld_scale <= MAX_SCHAR);
		save_desc.dsc_scale = (SCHAR) field->fld_scale;
		save_desc.dsc_sub_type = field->fld_sub_type;

		field->fld_dtype = 0;
		field->fld_length = 0;
		field->fld_scale = 0;
		field->fld_sub_type = 0;
	}

	PASS1_make_context(dsqlScratch, relation_node);

	dsql_nod* input = PASS1_node(dsqlScratch, node->nod_arg[e_cmp_expr]);

	// try to calculate size of the computed field. The calculated size
	// may be ignored, but it will catch self references
	dsc desc;
	MAKE_desc(dsqlScratch, &desc, input);

	// generate the blr expression

	dsqlScratch->beginBlr(isc_dyn_fld_computed_blr);
	GEN_expr(dsqlScratch, input);
	dsqlScratch->endBlr();

	if (save_desc.dsc_dtype)
	{
		// restore the field size/type overrides
		field->fld_dtype  = save_desc.dsc_dtype;
		field->fld_length = save_desc.dsc_length;
		field->fld_scale  = save_desc.dsc_scale;
		if (field->fld_dtype <= dtype_any_text)
		{
			field->fld_character_set_id = DSC_GET_CHARSET(&save_desc);
			field->fld_collation_id= DSC_GET_COLLATE(&save_desc);
		}
		else
			field->fld_sub_type = save_desc.dsc_sub_type;
	}
	else if (field)
	{
		// use size calculated
		field->fld_dtype  = desc.dsc_dtype;
		field->fld_length = desc.dsc_length;
		field->fld_scale  = desc.dsc_scale;
		if (field->fld_dtype <= dtype_any_text)
		{
			field->fld_character_set_id = DSC_GET_CHARSET(&desc);
			field->fld_collation_id= DSC_GET_COLLATE(&desc);
		}
		else
			field->fld_sub_type = desc.dsc_sub_type;
	}

	statement->setType(DsqlCompiledStatement::TYPE_DDL);
	statement->setDdlNode(saved_ddl_node);
	DDL_reset_context_stack(dsqlScratch);

	// generate the source text
	const dsql_str* source = (dsql_str*) node->nod_arg[e_cmp_text];
	fb_assert(source->str_length <= MAX_USHORT);
	dsqlScratch->appendString(isc_dyn_fld_computed_source, source->str_data,
		(USHORT) source->str_length);
}


static SSHORT getBlobFilterSubType(DsqlCompilerScratch* dsqlScratch, const dsql_nod* node)
{
/*******************************************
 *
 *	g e t B l o b F i l t e r S u b T y p e
 *
 *******************************************
 *
 * Function
 *	get sub_type value from LiteralNode.
 *
 **************************************/
 	const LiteralNode* literal = ExprNode::as<LiteralNode>(node);
 	fb_assert(literal);

	switch (literal->litDesc.dsc_dtype)
	{
	case dtype_long:
		return (SSHORT) literal->getSlong();
	case dtype_text:
		break;
	default:
		fb_assert(false);
		return 0;
	}

	// fall thru for dtype_text
	const dsql_str* type_name = reinterpret_cast<const dsql_str*>(node->nod_arg[0]);
	SSHORT blob_sub_type;
	if (!METD_get_type(dsqlScratch->getTransaction(), type_name, "RDB$FIELD_SUB_TYPE", &blob_sub_type))
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-204) <<
				  Arg::Gds(isc_dsql_datatype_err) <<
				  Arg::Gds(isc_dsql_blob_type_unknown) << Arg::Str(type_name->str_data));
	}
	return blob_sub_type;
}

static void define_filter(DsqlCompilerScratch* dsqlScratch)
{
/**************************************
 *
 *	d e f i n e _ f i l t e r
 *
 **************************************
 *
 * Function
 *	define a filter to the database.
 *
 **************************************/
	DsqlCompiledStatement* statement = dsqlScratch->getStatement();
	const dsql_nod* filter_node = statement->getDdlNode();
	const dsql_nod* const* ptr = filter_node->nod_arg;

	dsqlScratch->appendNullString(isc_dyn_def_filter, ((dsql_str*) (ptr[e_filter_name]))->str_data);
	dsqlScratch->appendNumber(isc_dyn_filter_in_subtype,
		getBlobFilterSubType(dsqlScratch, ptr[e_filter_in_type]));
	dsqlScratch->appendNumber(isc_dyn_filter_out_subtype,
		getBlobFilterSubType(dsqlScratch, ptr[e_filter_out_type]));
	dsqlScratch->appendNullString(isc_dyn_func_entry_point,
		((dsql_str*) (ptr[e_filter_entry_pt]))->str_data);
	dsqlScratch->appendNullString(isc_dyn_func_module_name,
		((dsql_str*) (ptr[e_filter_module]))->str_data);

	dsqlScratch->appendUChar(isc_dyn_end);
}


static void define_index(DsqlCompilerScratch* dsqlScratch)
{
/**************************************
 *
 *	d e f i n e _ i n d e x
 *
 **************************************
 *
 * Function
 *	Generate ddl to create an index.
 *
 **************************************/
	DsqlCompiledStatement* statement = dsqlScratch->getStatement();

	dsqlScratch->appendUChar(isc_dyn_begin);

	const dsql_nod* ddl_node = statement->getDdlNode();
	dsql_nod* relation_node = (dsql_nod*) ddl_node->nod_arg[e_idx_table];
	const MetaName& relation_name = ExprNode::as<RelationSourceNode>(relation_node)->dsqlName;
	dsql_nod* field_list = ddl_node->nod_arg[e_idx_fields];
	const dsql_str* index_name = (dsql_str*) ddl_node->nod_arg[e_idx_name];

	dsqlScratch->appendNullString(isc_dyn_def_idx, index_name->str_data);
	dsqlScratch->appendNullString(isc_dyn_rel_name, relation_name.c_str());

	// go through the fields list, making an index segment for each field,
	// unless we have a computation, in which case generate an expression index

	if (field_list->nod_type == nod_list)
	{
	    const dsql_nod* const* ptr = field_list->nod_arg;
	    const dsql_nod* const* const end = ptr + field_list->nod_count;
		for (; ptr < end; ptr++)
			dsqlScratch->appendNullString(isc_dyn_fld_name, ((dsql_str*) (*ptr)->nod_arg[1])->str_data);
	}
	else if (field_list->nod_type == nod_def_computed)
		define_computed(dsqlScratch, relation_node, NULL, field_list);

	// check for a unique index

	if (ddl_node->nod_arg[e_idx_unique]) {
		dsqlScratch->appendNumber(isc_dyn_idx_unique, 1);
	}

	if (ddl_node->nod_arg[e_idx_asc_dsc]) {
		dsqlScratch->appendNumber(isc_dyn_idx_type, 1);
	}

	dsqlScratch->appendUChar(isc_dyn_end);			// of define index
	dsqlScratch->appendUChar(isc_dyn_end);			// of begin
}


static void generate_dyn(DsqlCompilerScratch* dsqlScratch, dsql_nod* node)
{
/**************************************
 *
 *	g e n e r a t e _ d y n
 *
 **************************************
 *
 * Functional description
 *	Switch off the type of node to generate a
 *	DYN string.
 *
 **************************************/
	switch (node->nod_type)
	{
	case nod_def_index:
		define_index(dsqlScratch);
		break;

	case nod_grant:
	case nod_revoke:
		grant_revoke(dsqlScratch);
		break;

	case nod_def_filter:
		define_filter(dsqlScratch);
		break;

	case nod_add_user:
		define_user(dsqlScratch, isc_dyn_user_add);
		break;

	case nod_mod_user:
		define_user(dsqlScratch, isc_dyn_user_mod);
		break;

	default: // CVC: Shouldn't we complain here?
		break;
	}
}


static void grant_revoke(DsqlCompilerScratch* dsqlScratch)
{
/**************************************
 *
 *	g r a n t _ r e v o k e
 *
 **************************************
 *
 * Functional description
 *	Build DYN string for GRANT/REVOKE statements
 *
 **************************************/

	const dsql_nod* const* uptr;
	const dsql_nod* const* uend;

	SSHORT option = 0; // no grant/admin option
	DsqlCompiledStatement* statement = dsqlScratch->getStatement();
	dsql_nod* ddl_node = statement->getDdlNode();
	const dsql_nod* privs = ddl_node->nod_arg[e_grant_privs];
	const dsql_nod* table = ddl_node->nod_arg[e_grant_table];

	if ((ddl_node->nod_type == nod_revoke) && !privs && !table)	// ALL ON ALL
	{
		dsqlScratch->appendUChar(isc_dyn_begin);
		const dsql_nod* users = ddl_node->nod_arg[e_grant_users];
		uend = users->nod_arg + users->nod_count;
		for (uptr = users->nod_arg; uptr < uend; ++uptr)
		{
			dsqlScratch->appendUChar(isc_dyn_revoke_all);
			put_user_grant(dsqlScratch, *uptr);
			dsqlScratch->appendUChar(isc_dyn_end);
		}
		dsqlScratch->appendUChar(isc_dyn_end);

		return;
	}

	bool process_grant_role = false;
	if (privs->nod_arg[0] != NULL)
	{
		if (privs->nod_arg[0]->nod_type == nod_role_name) {
			process_grant_role = true;
		}
	}

	dsqlScratch->appendUChar(isc_dyn_begin);

	if (!process_grant_role)
	{
		const dsql_nod* users = ddl_node->nod_arg[e_grant_users];
		if (ddl_node->nod_arg[e_grant_grant]) {
			option = 1; // with grant option
		}

		uend = users->nod_arg + users->nod_count;
		for (uptr = users->nod_arg; uptr < uend; ++uptr)
		{
			modify_privileges(dsqlScratch, ddl_node->nod_type, option,
							  privs, table, *uptr, ddl_node->nod_arg[e_grant_grantor]);
		}
	}
	else
	{
		const dsql_nod* role_list = ddl_node->nod_arg[0];
		const dsql_nod* users = ddl_node->nod_arg[1];
		if (ddl_node->nod_arg[3]) {
			option = 2; // with admin option
		}

		const dsql_nod* const* role_end = role_list->nod_arg + role_list->nod_count;
		for (const dsql_nod* const* role_ptr = role_list->nod_arg; role_ptr < role_end; ++role_ptr)
		{
			uend = users->nod_arg + users->nod_count;
			for (uptr = users->nod_arg; uptr < uend; ++uptr)
			{
				process_role_nm_list(dsqlScratch, option, *uptr, *role_ptr,
									 ddl_node->nod_type, ddl_node->nod_arg[e_grant_grantor]);
			}
		}
	}

	dsqlScratch->appendUChar(isc_dyn_end);
}


static void put_user_grant(DsqlCompilerScratch* dsqlScratch, const dsql_nod* user)
{
/**************************************
 *
 *	p u t _ u s e r _ g r a n t
 *
 **************************************
 *
 * Functional description
 *	Stuff a user/role/obj option in grant/revoke
 *
 **************************************/
	const dsql_str* name = (dsql_str*) user->nod_arg[0];

	switch (user->nod_type)
	{
	case nod_user_group:		// GRANT priv ON tbl TO GROUP unix_group
		dsqlScratch->appendNullString(isc_dyn_grant_user_group, name->str_data);
		break;

	case nod_user_name:
		if (user->nod_count == 2)
		   dsqlScratch->appendNullString(isc_dyn_grant_user_explicit, name->str_data);
		else
			dsqlScratch->appendNullString(isc_dyn_grant_user, name->str_data);
		break;

	case nod_package_obj:
		dsqlScratch->appendNullString(isc_dyn_grant_package, name->str_data);
		break;

	case nod_proc_obj:
		dsqlScratch->appendNullString(isc_dyn_grant_proc, name->str_data);
		break;

	case nod_func_obj:
		dsqlScratch->appendNullString(isc_dyn_grant_func, name->str_data);
		break;

	case nod_trig_obj:
		dsqlScratch->appendNullString(isc_dyn_grant_trig, name->str_data);
		break;

	case nod_view_obj:
		dsqlScratch->appendNullString(isc_dyn_grant_view, name->str_data);
		break;

	case nod_role_name:
		dsqlScratch->appendNullString(isc_dyn_grant_role, name->str_data);
		break;

	default:
		// CVC: Here we should complain: DYN doesn't check parameters
		// and it will write trash in rdb$user_privileges. We probably
		// should complain in most cases when "name" is blank, too.
		break;
	}
}


static void modify_privilege(DsqlCompilerScratch* dsqlScratch,
							 NOD_TYPE type,
							 SSHORT option,
							 const UCHAR* privs,
							 const dsql_nod* table,
							 const dsql_nod* user,
							 const dsql_nod* grantor,
							 const dsql_str* field_name)
{
/**************************************
 *
 *	m o d i f y _ p r i v i l e g e
 *
 **************************************
 *
 * Functional description
 *	Stuff a single grant/revoke verb and all its options.
 *
 **************************************/

	if (type == nod_grant)
		dsqlScratch->appendUChar(isc_dyn_grant);
	else
		dsqlScratch->appendUChar(isc_dyn_revoke);

	// stuff the privileges string

	SSHORT priv_count = 0;
	dsqlScratch->appendUShort(0);

	for (; *privs; privs++)
	{
		priv_count++;
		dsqlScratch->appendUChar(*privs);
	}

	UCHAR* dynsave = dsqlScratch->getBlrData().end();
	for (SSHORT i = priv_count + 2; i; i--)
		--dynsave;

	*dynsave++ = (UCHAR) priv_count;
	*dynsave = (UCHAR) (priv_count >> 8);

	UCHAR dynVerb = 0;

	switch (table->nod_type)
	{
	case nod_procedure_name:
		dynVerb = isc_dyn_prc_name;
		break;
	case nod_function_name:
		dynVerb = isc_dyn_fun_name;
		break;
	case nod_package_name:
		dynVerb = isc_dyn_pkg_name;
		break;
	default:
		dynVerb = isc_dyn_rel_name;
	}

	const char* name = dynVerb == isc_dyn_rel_name ?
		ExprNode::as<RelationSourceNode>(table)->dsqlName.c_str() :
		((dsql_str*) table->nod_arg[0])->str_data;

	dsqlScratch->appendNullString(dynVerb, name);

	put_user_grant(dsqlScratch, user);

	if (field_name)
		dsqlScratch->appendNullString(isc_dyn_fld_name, field_name->str_data);

	if (option)
		dsqlScratch->appendNumber(isc_dyn_grant_options, option);

	put_grantor(dsqlScratch, grantor);

	dsqlScratch->appendUChar(isc_dyn_end);
}



static char modify_privileges(DsqlCompilerScratch* dsqlScratch,
							   NOD_TYPE type,
							   SSHORT option,
							   const dsql_nod* privs,
							   const dsql_nod* table,
							   const dsql_nod* user,
							   const dsql_nod* grantor)
{
/**************************************
 *
 *	m o d i f y _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *     Return a char indicating the privilege to be modified
 *
 **************************************/

	char privileges[10];
	const char* p = 0;
	char* q;
	const dsql_nod* fields;
	const dsql_nod* const* ptr;
	const dsql_nod* const* end;

	switch (privs->nod_type)
	{
	case nod_all:
		p = "A";
		break;

	case nod_select:
		return 'S';

	case nod_execute:
		return 'X';

	case nod_insert:
		return 'I';

	case nod_references:
	case nod_update:
		p = (privs->nod_type == nod_references) ? "R" : "U";
		fields = privs->nod_arg[0];
		if (!fields) {
			return *p;
		}

		for (ptr = fields->nod_arg, end = ptr + fields->nod_count; ptr < end; ptr++)
		{
			modify_privilege(dsqlScratch, type, option,
							 reinterpret_cast<const UCHAR*>(p), table, user, grantor,
							 reinterpret_cast<dsql_str*>((*ptr)->nod_arg[1]));
		}
		return 0;

	case nod_delete:
		return 'D';

	case nod_list:
		p = q = privileges;
		for (ptr = privs->nod_arg, end = ptr + privs->nod_count; ptr < end; ptr++)
		{
			*q = modify_privileges(dsqlScratch, type, option, *ptr, table, user, grantor);
			if (*q) {
				q++;
			}
		}
		*q = 0;
		break;

	default:
		break;
	}

	if (*p)
	{
		modify_privilege(dsqlScratch, type, option,
						 reinterpret_cast<const UCHAR*>(p), table, user, grantor, 0);
	}

	return 0;
}


// *********************
// d e f i n e _ u s e r
// *********************
// Support SQL operator create/alter/drop user
static void define_user(DsqlCompilerScratch* dsqlScratch, UCHAR op)
{
	DsqlCompiledStatement* statement = dsqlScratch->getStatement();

	dsqlScratch->appendUChar(isc_dyn_user);

	const dsql_nod* node = statement->getDdlNode();
	int argCount = 0;

	for (int i = 0; i < node->nod_count; ++i)
	{
		const dsql_str* ds = (dsql_str*) node->nod_arg[i];
		if (! ds)
		{
			if (i == e_user_name || (i == e_user_passwd && op == isc_dyn_user_add))
			{
				ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
						  // Unexpected end of command
						  Arg::Gds(isc_command_end_err2) << Arg::Num(node->nod_line) <<
															Arg::Num(node->nod_column));
			}

			continue;
		}

		++argCount;

		switch (i)
		{
		case e_user_name:
			dsqlScratch->appendNullString(op, ds->str_data);
			break;
		case e_user_passwd:
			dsqlScratch->appendNullString(isc_dyn_user_passwd, ds->str_data);
			break;
		case e_user_first:
			dsqlScratch->appendNullString(isc_dyn_user_first, ds->str_data);
			break;
		case e_user_middle:
			dsqlScratch->appendNullString(isc_dyn_user_middle, ds->str_data);
			break;
		case e_user_last:
			dsqlScratch->appendNullString(isc_dyn_user_last, ds->str_data);
			break;
		case e_user_admin:
			dsqlScratch->appendNullString(isc_dyn_user_admin, ds->str_data);
			break;
		}
	}

	if (argCount < 2)
	{
		ERRD_post(Arg::Gds(isc_sqlerr) << Arg::Num(-104) <<
				  // Unexpected end of command
				  Arg::Gds(isc_command_end_err2) << Arg::Num(node->nod_line) <<
													Arg::Num(node->nod_column));
	}

	dsqlScratch->appendUChar(isc_user_end);
	dsqlScratch->appendUChar(isc_dyn_end);
}


static void process_role_nm_list(DsqlCompilerScratch* dsqlScratch,
								 SSHORT option,
								 const dsql_nod* user_ptr,
								 const dsql_nod* role_ptr,
								 NOD_TYPE type,
								 const dsql_nod* grantor)
{
/**************************************
 *
 *  p r o c e s s _ r o l e _ n m _ l i s t
 *
 **************************************
 *
 * Functional description
 *     Build req_blr for grant & revoke role stmt
 *
 **************************************/
	if (type == nod_grant)
		dsqlScratch->appendUChar(isc_dyn_grant);
	else
		dsqlScratch->appendUChar(isc_dyn_revoke);

	dsqlScratch->appendUShort(1);
	dsqlScratch->appendUChar('M');

	const dsql_str* role_nm = (dsql_str*) role_ptr->nod_arg[0];
	dsqlScratch->appendNullString(isc_dyn_sql_role_name, role_nm->str_data);

	const dsql_str* user_nm = (dsql_str*) user_ptr->nod_arg[0];
	dsqlScratch->appendNullString(isc_dyn_grant_user, user_nm->str_data);

	if (option) {
		dsqlScratch->appendNumber(isc_dyn_grant_admin_options, option);
	}

	put_grantor(dsqlScratch, grantor);

	dsqlScratch->appendUChar(isc_dyn_end);
}


static void put_grantor(DsqlCompilerScratch* dsqlScratch, const dsql_nod* grantor)
{
/**************************************
 *
 *	p u t _ g r a n t o r
 *
 **************************************
 *
 * Function
 *	Write out grantor for grant / revoke.
 *
 **************************************/
	if (grantor)
	{
		fb_assert(grantor->nod_type == nod_user_name);
		const dsql_str* name = (const dsql_str*) grantor->nod_arg[0];
		dsqlScratch->appendNullString(isc_dyn_grant_grantor, name->str_data);
	}
}


void DDL_reset_context_stack(DsqlCompilerScratch* dsqlScratch)
{
/**************************************
 *
 *	D D L _ r e s e t _ c o n t e x t _ s t a c k
 *
 **************************************
 *
 * Function
 *	Get rid of any predefined contexts created
 *	for a view or trigger definition.
 *	Also reset hidden variables.
 *
 **************************************/

	dsqlScratch->context->clear();
	dsqlScratch->contextNumber = 0;
	dsqlScratch->derivedContextNumber = 0;

	dsqlScratch->hiddenVarsNumber = 0;
	dsqlScratch->hiddenVariables.clear();
}


// post very often used error - avoid code duplication
static void post_607(const Arg::StatusVector& v)
{
	Arg::Gds err(isc_sqlerr);
	err << Arg::Num(-607) << Arg::Gds(isc_dsql_command_err);

	err.append(v);
	ERRD_post(err);
}

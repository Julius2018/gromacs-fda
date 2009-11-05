/*
 *
 *                This source code is part of
 *
 *                 G   R   O   M   A   C   S
 *
 *          GROningen MAchine for Chemical Simulations
 *
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2009, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 *
 * For more info, check our website at http://www.gromacs.org
 */
/*! \internal \file
 * \brief Implementation of the merging selection modifier.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <macros.h>
#include <smalloc.h>
#include <vec.h>

#include <position.h>
#include <selmethod.h>

/*! \internal \brief
 * Data structure for the merging selection modifiers.
 */
typedef struct
{
    /** Input positions. */
    gmx_ana_pos_t    p1;
    /** Other input positions. */
    gmx_ana_pos_t    p2;
    /** Group to store the output atom indices. */
    gmx_ana_index_t  g;
    /** Stride for merging (\c stride values from \c p1 for each in \c p2). */
    int              stride;
} t_methoddata_merge;

/** Allocates data for the merging selection modifiers. */
static void *
init_data_merge(int npar, gmx_ana_selparam_t *param);
/** Initializes data for the merging selection modifiers. */
static int
init_merge(t_topology *top, int npar, gmx_ana_selparam_t *param, void *data);
/** Initializes output for the \p merge selection modifier. */
static int
init_output_merge(t_topology *top, gmx_ana_selvalue_t *out, void *data);
/** Initializes output for the \p plus selection modifier. */
static int
init_output_plus(t_topology *top, gmx_ana_selvalue_t *out, void *data);
/** Frees the memory allocated for the merging selection modifiers. */
static void
free_data_merge(void *data);
/** Evaluates the \p merge selection modifier. */
static int
evaluate_merge(t_topology *top, t_trxframe *fr, t_pbc *pbc,
               gmx_ana_pos_t *p, gmx_ana_selvalue_t *out, void *data);
/** Evaluates the \p plus selection modifier. */
static int
evaluate_plus(t_topology *top, t_trxframe *fr, t_pbc *pbc,
              gmx_ana_pos_t *p, gmx_ana_selvalue_t *out, void *data);

/** Parameters for the merging selection modifiers. */
static gmx_ana_selparam_t smparams_merge[] = {
    {NULL,       {POS_VALUE, -1, {NULL}}, NULL, SPAR_DYNAMIC | SPAR_VARNUM},
    {NULL,       {POS_VALUE, -1, {NULL}}, NULL, SPAR_DYNAMIC | SPAR_VARNUM},
    {"stride",   {INT_VALUE,  1, {NULL}}, NULL, SPAR_OPTIONAL},
};

/** Help text for the merging selection modifiers. */
static const char *help_merge[] = {
    "MERGING SELECTIONS[PAR]",

    "[TT]POSEXPR merge POSEXPR [stride INT][tt][BR]",
    "[TT]POSEXPR merge POSEXPR [merge POSEXPR ...][tt][BR]",
    "[TT]POSEXPR plus POSEXPR [plus POSEXPR ...][tt][PAR]",

    "Basic selection keywords can only create selections where each atom",
    "occurs at most once. The [TT]merge[tt] and [TT]plus[tt] selection",
    "keywords can be used to work around this limitation. Both create",
    "a selection that contains the positions from all the given position",
    "expressions, even if they contain duplicates.",
    "The difference between the two is that [TT]merge[tt] expects two or more",
    "selections with the same number of positions, and the output contains",
    "the input positions selected from each expression in turn, i.e.,",
    "the output is like A1 B1 A2 B2 and so on. It is also possible to merge",
    "selections of unequal size as long as the size of the first is a",
    "multiple of the second one. The [TT]stride[tt] parameter can be used",
    "to explicitly provide this multiplicity.",
    "[TT]plus[tt] simply concatenates the positions after each other, and",
    "can work also with selections of different sizes.",
    "These keywords are valid only at the selection level, not in any",
    "subexpressions.[PAR]",
};

/** \internal Selection method data for the \p plus modifier. */
gmx_ana_selmethod_t sm_merge = {
    "merge", POS_VALUE, SMETH_MODIFIER,
    asize(smparams_merge), smparams_merge,
    &init_data_merge,
    NULL,
    &init_merge,
    &init_output_merge,
    &free_data_merge,
    NULL,
    NULL,
    &evaluate_merge,
    {"merge POSEXPR", asize(help_merge), help_merge},
};

/** \internal Selection method data for the \p plus modifier. */
gmx_ana_selmethod_t sm_plus = {
    "plus", POS_VALUE, SMETH_MODIFIER,
    asize(smparams_merge)-1, smparams_merge,
    &init_data_merge,
    NULL,
    &init_merge,
    &init_output_plus,
    &free_data_merge,
    NULL,
    NULL,
    &evaluate_plus,
    {"plus POSEXPR", asize(help_merge), help_merge},
};

/*!
 * \param[in]     npar  Should be 2 for \c plus and 3 for \c merge.
 * \param[in,out] param Method parameters (should point to a copy of
 *   \ref smparams_merge).
 * \returns Pointer to the allocated data (\p t_methoddata_merge).
 *
 * Allocates memory for a \p t_methoddata_merge structure.
 */
static void *
init_data_merge(int npar, gmx_ana_selparam_t *param)
{
    t_methoddata_merge *data;

    snew(data, 1);
    data->stride = 0;
    param[0].val.u.p = &data->p1;
    param[1].val.u.p = &data->p2;
    if (npar > 2)
    {
        param[2].val.u.i = &data->stride;
    }
    return data;
}

/*!
 * \param[in] top   Not used.
 * \param[in] npar  Not used (should be 2 or 3).
 * \param[in] param Method parameters (should point to \ref smparams_merge).
 * \param[in] data  Should point to a \p t_methoddata_merge.
 * \returns   0 if everything is successful, -1 on error.
 */
static int
init_merge(t_topology *top, int npar, gmx_ana_selparam_t *param, void *data)
{
    t_methoddata_merge *d = (t_methoddata_merge *)data;
    int                 i;

    if (d->stride < 0)
    {
        fprintf(stderr, "error: stride for merging should be positive\n");
        return -1;
    }
    gmx_ana_index_reserve(&d->g, d->p1.g->isize + d->p2.g->isize);
    d->g.isize = d->p1.g->isize + d->p2.g->isize;
    return 0;
}

/*! \brief
 * Does common initialization to all merging modifiers.
 *
 * \param[in]     top   Topology data structure.
 * \param[in,out] out   Pointer to output data structure.
 * \param[in,out] data  Should point to \c t_methoddata_merge.
 * \returns       0 for success.
 */
static int
init_output_common(t_topology *top, gmx_ana_selvalue_t *out, void *data)
{
    t_methoddata_merge *d = (t_methoddata_merge *)data;

    if (d->p1.m.type != d->p2.m.type)
    {
        /* TODO: Maybe we could pick something else here? */
        out->u.p->m.type = INDEX_UNKNOWN;
    }
    else
    {
        out->u.p->m.type = d->p1.m.type;
    }
    gmx_ana_pos_reserve(out->u.p, d->p1.nr + d->p2.nr, d->g.isize);
    out->u.p->nr           = d->p1.nr + d->p2.nr;
    out->u.p->m.nr         = out->u.p->nr;
    out->u.p->m.mapb.nr    = out->u.p->nr;
    out->u.p->m.b.nr       = out->u.p->nr;
    out->u.p->m.b.nra      = d->g.isize;
    out->u.p->m.bStatic    = d->p1.m.bStatic && d->p2.m.bStatic;
    out->u.p->m.bMapStatic = d->p1.m.bMapStatic && d->p2.m.bMapStatic;
    out->u.p->g = &d->g;
    d->g.isize = 0;
    out->u.p->nr = 0;
    out->u.p->m.mapb.index[0] = 0;
    out->u.p->m.b.index[0]    = 0;
    return 0;
}

/*! \brief
 * Helper function to initializes a single output position.
 *
 * \param[in,out] out  Data structure to which the new position is appended.
 * \param[in,out] g    Data structure to which the new atoms are appended.
 * \param[in]     from Data structure from which the position is copied.
 * \param[in]     i    Index in \p from to copy.
 */
static void
init_output_append_pos(gmx_ana_pos_t *out, gmx_ana_index_t *g,
                       gmx_ana_pos_t *from, int i)
{
    int  j, k;

    j = out->nr;
    copy_rvec(from->x[i], out->x[j]);
    out->m.refid[j] = j;
    out->m.mapid[j] = from->m.mapid[i];
    out->m.orgid[j] = from->m.orgid[i];
    for (k = from->m.mapb.index[i]; k < from->m.mapb.index[i+1]; ++k)
    {
        g->index[g->isize]   = from->g->index[k];
        out->m.b.a[g->isize] = from->m.b.a[k];
        g->isize++;
    }
    out->m.mapb.index[j+1] = g->isize;
    out->m.b.index[j+1]    = g->isize;
    out->nr++;
}

/*!
 * \param[in]     top   Topology data structure.
 * \param[in,out] out   Pointer to output data structure.
 * \param[in,out] data  Should point to \c t_methoddata_merge.
 * \returns       0 for success.
 */
static int
init_output_merge(t_topology *top, gmx_ana_selvalue_t *out, void *data)
{
    t_methoddata_merge *d = (t_methoddata_merge *)data;
    int                 i, j;

    /* If no stride given, deduce it from the input sizes */
    if (d->stride == 0)
    {
        d->stride = d->p1.nr / d->p2.nr;
    }
    if (d->p1.nr != d->stride*d->p2.nr)
    {
        fprintf(stderr, "error: the number of positions to be merged are not compatible\n");
        return -1;
    }
    init_output_common(top, out, data);
    for (i = 0; i < d->p2.nr; ++i)
    {
        for (j = 0; j < d->stride; ++j)
        {
            init_output_append_pos(out->u.p, &d->g, &d->p1, d->stride*i+j);
        }
        init_output_append_pos(out->u.p, &d->g, &d->p2, i);
    }
    return 0;
}

/*!
 * \param[in]     top   Topology data structure.
 * \param[in,out] out   Pointer to output data structure.
 * \param[in,out] data  Should point to \c t_methoddata_merge.
 * \returns       0 for success.
 */
static int
init_output_plus(t_topology *top, gmx_ana_selvalue_t *out, void *data)
{
    t_methoddata_merge *d = (t_methoddata_merge *)data;
    int                 i;

    init_output_common(top, out, data);
    for (i = 0; i < d->p1.nr; ++i)
    {
        init_output_append_pos(out->u.p, &d->g, &d->p1, i);
    }
    for (i = 0; i < d->p2.nr; ++i)
    {
        init_output_append_pos(out->u.p, &d->g, &d->p2, i);
    }
    return 0;
}

/*!
 * \param data Data to free (should point to a \p t_methoddata_merge).
 *
 * Frees the memory allocated for \c t_methoddata_merge.
 */
static void
free_data_merge(void *data)
{
    t_methoddata_merge *d = (t_methoddata_merge *)data;

    gmx_ana_pos_deinit(&d->p1);
    gmx_ana_pos_deinit(&d->p2);
    gmx_ana_index_deinit(&d->g);
}

/*! \brief
 * Helper function to perform common initialization before evaluation.
 *
 * \param[out] out   Output data structure (\p out->u.p is used).
 * \param[in]  data  Should point to a \p t_methoddata_merge.
 */
static void
evaluate_common(gmx_ana_selvalue_t *out, void *data)
{
    t_methoddata_merge *d = (t_methoddata_merge *)data;

    out->u.p->nr              = d->p1.nr + d->p2.nr;
    out->u.p->m.nr            = out->u.p->nr;
    out->u.p->m.mapb.nr       = out->u.p->nr;
    out->u.p->m.bStatic       = d->p1.m.bStatic && d->p2.m.bStatic;
    out->u.p->m.bMapStatic    = d->p1.m.bMapStatic && d->p2.m.bMapStatic;
    d->g.isize                = 0;
    out->u.p->nr              = 0;
    out->u.p->m.mapb.index[0] = 0;
}

/*! \brief
 * Helper function to evaluate a single output position.
 *
 * \param[in,out] out   Data structure to which the new position is appended.
 * \param[in,out] g     Data structure to which the new atoms are appended.
 * \param[in]     from  Data structure from which the position is copied.
 * \param[in]     i     Index in \p from to copy.
 * \param[in]     refid Reference ID in \p out
 *   (all negative values are treated as -1).
 */
static void
evaluate_append_pos(gmx_ana_pos_t *out, gmx_ana_index_t *g,
                    gmx_ana_pos_t *from, int i, int refid)
{
    int  j, k;

    j = out->nr;
    copy_rvec(from->x[i], out->x[j]);
    if (refid < 0)
    {
        out->m.refid[j] = -1;
    }
    else
    {
        out->m.refid[j] = refid;
        out->m.mapid[j] = out->m.orgid[refid];
    }
    for (k = from->m.mapb.index[i]; k < from->m.mapb.index[i+1]; ++k)
    {
        g->index[g->isize++] = from->g->index[k];
    }
    out->m.mapb.index[j+1] = g->isize;
    out->nr++;
}

/*!
 * \param[in]  top   Not used.
 * \param[in]  fr    Not used.
 * \param[in]  pbc   Not used.
 * \param[in]  p     Positions to merge (should point to \p data->p1).
 * \param[out] out   Output data structure (\p out->u.p is used).
 * \param[in]  data  Should point to a \p t_methoddata_merge.
 * \returns    0 on success.
 */
static int
evaluate_merge(t_topology *top, t_trxframe *fr, t_pbc *pbc,
               gmx_ana_pos_t *p, gmx_ana_selvalue_t *out, void *data)
{
    t_methoddata_merge *d = (t_methoddata_merge *)data;
    int                 i, j;
    int                 refid;

    if (d->p1.nr != d->stride*d->p2.nr)
    {
        fprintf(stderr, "error: the number of positions to be merged are not compatible\n");
        return -1;
    }
    evaluate_common(out, data);
    for (i = 0; i < d->p2.nr; ++i)
    {
        for (j = 0; j < d->stride; ++j)
        {
            refid = d->p1.m.refid[d->stride*i+j];
            if (refid != -1)
            {
                refid = (d->stride+1) * (refid / d->stride) + (refid % d->stride);
            }
            evaluate_append_pos(out->u.p, &d->g, &d->p1, d->stride*i+j, refid);
        }
        refid = (d->stride+1)*d->p2.m.refid[i]+d->stride;
        evaluate_append_pos(out->u.p, &d->g, &d->p2, i, refid);
    }
    return 0;
}

/*!
 * \param[in]  top   Not used.
 * \param[in]  fr    Not used.
 * \param[in]  pbc   Not used.
 * \param[in]  p     Positions to merge (should point to \p data->p1).
 * \param[out] out   Output data structure (\p out->u.p is used).
 * \param[in]  data  Should point to a \p t_methoddata_merge.
 * \returns    0 on success.
 */
static int
evaluate_plus(t_topology *top, t_trxframe *fr, t_pbc *pbc,
              gmx_ana_pos_t *p, gmx_ana_selvalue_t *out, void *data)
{
    t_methoddata_merge *d = (t_methoddata_merge *)data;
    int                 i;
    int                 refid;

    evaluate_common(out, data);
    for (i = 0; i < d->p1.nr; ++i)
    {
        refid = d->p1.m.refid[i];
        evaluate_append_pos(out->u.p, &d->g, &d->p1, i, refid);
    }
    for (i = 0; i < d->p2.nr; ++i)
    {
        refid = d->p2.m.refid[i];
        if (refid != -1)
        {
            refid += d->p1.m.b.nr;
        }
        evaluate_append_pos(out->u.p, &d->g, &d->p2, i, refid);
    }
    return 0;
}
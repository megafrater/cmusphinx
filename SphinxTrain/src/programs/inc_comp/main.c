/* -*- c-basic-offset: 4 -*- */
/* ====================================================================
 * Copyright (c) 1994-2000 Carnegie Mellon University.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*********************************************************************
 *
 * File: main.c
 * 
 * Description: 
 * 	For every tied mixture, create a new component density.
 *
 * Background:
 *
 * Author: 
 * 	Eric H. Thayer (eht@cs.cmu.edu)
 *********************************************************************/

#include "parse_cmd_ln.h"
#include "accum_wt_param.h"
#include "inc_densities.h"

#include <s3/vector.h>
#include <s3/gauden.h>
#include <s3/s3gau_io.h>
#include <s3/s3mixw_io.h>
#include <s3/feat.h>
#include <s3/ckd_alloc.h>
#include <s3/cmd_ln.h>
#include <s3/s3.h>

int
main(int argc, char *argv[])
{
    vector_t ***mean;
    vector_t ***var = NULL;
    vector_t ****fullvar = NULL;
    vector_t ***new_mean;
    vector_t ***new_var = NULL;
    vector_t ****new_fullvar = NULL;
    float32  ***dnom;
    float32 ***mixw;
    float32 ***new_mixw;
    uint32 n_mixw;
    uint32 n_mgau;
    uint32 n_feat;
    uint32 n_density;
    uint32 n_inc;
    const uint32 *veclen;
    int32 var_is_full;

    parse_cmd_ln(argc, argv);

    E_INFO("Reading mixing weight file %s.\n",
	   cmd_ln_access("-inmixwfn"));

    if (s3mixw_read((const char *)cmd_ln_access("-inmixwfn"),
		    &mixw,
		    &n_mixw,
		    &n_feat,
		    &n_density) != S3_SUCCESS) {
	return 1;
    }

    n_inc = *(int32 *)cmd_ln_access("-ninc");

    if (n_inc > n_density) {
	E_WARN("# of densities to split (== %u) > total # of densities/mixture (== %u); # split <- %u # den/mix\n",
	       n_inc, n_density, n_density);
	n_inc = n_density;
    }
    
    if (s3gau_read((const char *)cmd_ln_access("-inmeanfn"),
		   &mean,
		   &n_mgau,
		   &n_feat,
		   &n_density,
		   &veclen) != S3_SUCCESS) {
	return 1;
    }

    var_is_full = cmd_ln_int32("-fullvar");
    if (var_is_full) {
	if (s3gau_read_full((const char *)cmd_ln_access("-invarfn"),
			    &fullvar,
			    &n_mgau,
			    &n_feat,
			    &n_density,
			    &veclen) != S3_SUCCESS) {
	    return 1;
	}
    }
    else {
	if (s3gau_read((const char *)cmd_ln_access("-invarfn"),
		       &var,
		       &n_mgau,
		       &n_feat,
		       &n_density,
		       &veclen) != S3_SUCCESS) {
	    return 1;
	}
    }

    if (s3gaudnom_read((const char *)cmd_ln_access("-dcountfn"),
		       &dnom,
		       &n_mgau,
		       &n_feat,
		       &n_density) != S3_SUCCESS) {
	return 1;
    }
	
    new_mean = gauden_alloc_param(n_mgau, n_feat, n_density+n_inc, veclen);
    if (var_is_full)
	    new_fullvar  = gauden_alloc_param_full(n_mgau, n_feat, n_density+n_inc, veclen);
    else
	    new_var  = gauden_alloc_param(n_mgau, n_feat, n_density+n_inc, veclen);
    new_mixw = (float32 ***)ckd_calloc_3d(n_mixw, n_feat, n_density+n_inc,
					  sizeof(float32));
    
    E_INFO("output n_density == %u\n", n_density+n_inc);

    inc_densities(new_mixw,
		  new_mean,
		  new_var,
		  new_fullvar,

		  mixw,
		  mean,
		  var,
		  fullvar,
		  dnom,

		  n_mixw,
		  n_mgau,
		  n_feat,
		  n_density,
		  veclen,

		  n_inc);

    if (cmd_ln_access("-outmixwfn") != NULL) {
	if (s3mixw_write((char *)cmd_ln_access("-outmixwfn"),
			 new_mixw,
			 n_mixw,
			 n_feat,
			 n_density+n_inc) != S3_SUCCESS) {
	    return 1;
	}
    }
    else {
	E_FATAL("You must use the -outmixwfn argument\n");
    }

    if (cmd_ln_access("-outmeanfn") != NULL) {
	if (s3gau_write((const char *)cmd_ln_access("-outmeanfn"),
			(const vector_t ***)new_mean,
			n_mgau,
			n_feat,
			n_density+n_inc,
			veclen) != S3_SUCCESS) {
	    return 1;
	}
    }
    else {
	E_FATAL("You must use the -outmeanfn argument\n");
    }

    if (cmd_ln_access("-outvarfn") != NULL) {
	if (var_is_full) {
	    if (s3gau_write_full((const char *)cmd_ln_access("-outvarfn"),
				 (const vector_t ****)new_fullvar,
				 n_mgau,
				 n_feat,
				 n_density+n_inc,
				 veclen) != S3_SUCCESS) {
		return 1;
	    }
	}
	else {
	    if (s3gau_write((const char *)cmd_ln_access("-outvarfn"),
			    (const vector_t ***)new_var,
			    n_mgau,
			    n_feat,
			    n_density+n_inc,
			    veclen) != S3_SUCCESS) {
		return 1;
	    }
	}
    }
    else {
	E_FATAL("You must use the -outvarfn argument\n");
    }

    return 0;
}

/*
 * Log record.  Maintained by RCS.
 *
 * $Log$
 * Revision 1.4  2004/07/21  18:30:34  egouvea
 * Changed the license terms to make it the same as sphinx2 and sphinx3.
 * 
 * Revision 1.3  2001/04/05 20:02:31  awb
 * *** empty log message ***
 *
 * Revision 1.2  2000/09/29 22:35:14  awb
 * *** empty log message ***
 *
 * Revision 1.1  2000/09/24 21:38:31  awb
 * *** empty log message ***
 *
 * Revision 1.5  97/07/16  11:36:22  eht
 * *** empty log message ***
 * 
 * Revision 1.4  1996/08/06  14:07:14  eht
 * Deal w/ 2 pass variance computation
 *
 * Revision 1.3  1996/07/29  16:26:54  eht
 * development release
 *
 * Revision 1.2  1996/01/26  18:22:55  eht
 * Development version
 *
 * Revision 1.1  1995/12/15  18:37:07  eht
 * Initial revision
 *
 *
 */

/****************************************************************************
 *
 * MODULE:       orthophoto rectification program
 * AUTHOR(S):    Mike Baba of DBA Systems, Fairfax, VA for CERL
 * PURPOSE:      ortho-rectification of aerial photographs
 * COPYRIGHT:    (C) 1999 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/

/* orthoref.c */

/***********************************************************************
 * I_compute_ortho_equations()
 * I_orthoref()
 * I_inverse_orthoref()
 ***********************************************************************/
/*
   Main algorithm reference:

   Elements of Photogrammetry, With Air Photo Interpretation and Remote Sensing
   by Paul R. Wolf, 562 pages
   Publisher: McGraw Hill Text; 2nd edition (January 1983)
 */

#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <grass/imagery.h>
#include <grass/glocale.h>
#include "orthophoto.h"
#include "matrixdefs.h"
#include "local_proto.h"

#define MAX_ITERS  1000 /* Max iteration is normal equation solution */
#define CONV_LIMIT 0.1  /* meters */

#ifdef DEBUG
FILE *debug;
char msg[120];
#endif

static int panorama = 0;
static double ellps_a = 0;

/* enable panorama camera correction,
 * e.g. for CORONA KH-4A/B */
void I_ortho_panorama(void)
{
    panorama = 1;
}

/* Compute the ortho rectification parameters */
/* Camera position: XC, YC, ZC */
/* Camera attitude: Omega, Phi, Kappa */
int I_compute_ortho_equations(struct Ortho_Control_Points *cpz,
                              struct Ortho_Camera_File_Ref *cam_info,
                              struct Ortho_Camera_Exp_Init *init_info,
                              double *XC, double *YC, double *ZC, double *Omega,
                              double *Phi, double *Kappa, MATRIX *MO,
                              MATRIX *MT)
{
    MATRIX delta, epsilon, B, BT, C, E, N, CC, NN, UVW, XYZ, M, WT1;
    double x, y, /* z, */ X, Y, Z, Xp, Yp, CFL;
    double lam, mu, nu, U, V, W;
    double xbar, ybar;
    double m11, m12, m13, m21, m22, m23, m31, m32, m33;
    double sw, cw, sp, cp, sk, ck;
    double dx, dy, dz, dd;
    double Q1;
    double kappa1, kappa2, XC_var, YC_var, ZC_var;
    double omega_var, phi_var, kappa_var;
    int i, iter, max_iters, n;
    int first, active;
    double e_a, e2;

    Q1 = (double)1.0;

    if (!G_get_ellipsoid_parameters(&e_a, &e2))
        G_fatal_error(_("No ellpsoid parameters available"));
    ellps_a = e_a;

    /* DEBUG */
#ifdef DEBUG
    debug = fopen("ortho_compute.rst", "w");
    if (debug == NULL) {
        snprintf(msg, sizeof(msg),
                 "Cannot open debug file ortho_compute.rst\n");
        G_fatal_error(msg);
    }
#endif

    /*  Need at least 4 active control points,
     *  or user-provided camera position */
    active = 0;
    if (cpz) {
        for (i = 0; i < cpz->count; i++) {
            if (cpz->status[i] > 0)
                active++;
        }
    }

    /*  Initialize (and zero out) all matrices needed */
    /*  Format is delta = [XC,YC,ZC,Omega,Phi,Kappa]-transpose  */

    /*  Normal Equation Matrix */
    N.nrows = 6;
    N.ncols = 6;
    zero(&N);
    /*  Sum of Normal Equation Matrix */
    NN.nrows = 6;
    NN.ncols = 6;
    zero(&NN);
    /*  Partial derivates of observation equations */
    B.nrows = 2;
    B.ncols = 6;
    zero(&B);
    /*  Transpose of B */
    BT.nrows = 6;
    BT.ncols = 2;
    zero(&BT);
    /*  Partial solution matrix */
    C.nrows = 6;
    C.ncols = 1;
    zero(&C);
    /*  Sum of Partial solution matrix */
    CC.nrows = 6;
    CC.ncols = 1;
    zero(&CC);
    /*  Residual matrix */
    E.nrows = 2;
    E.ncols = 1;
    zero(&E);
    /*  delta Matrix  - [XC,YC,ZC,Omega,Phi,Kappa]-transpose */
    delta.nrows = 6;
    delta.ncols = 1;
    zero(&delta);
    /*  corrections to delta matrix */
    epsilon.nrows = 6;
    epsilon.ncols = 1;
    zero(&epsilon);
    /*  Object Space Coordinates (X,Y,Z) */
    XYZ.nrows = 3;
    XYZ.ncols = 1;
    zero(&XYZ);
    /*  Image Space coordinates  (u,v,w) */
    UVW.nrows = 3;
    UVW.ncols = 1;
    zero(&UVW);
    /*  Orientation Matrix  M=[3,3] functions of (omega,phi,kappa) */
    M.nrows = 3;
    M.ncols = 3;
    zero(&M);
    /*  Weight Matrix for delta */
    /*  Weights set to identity matrix unless */
    WT1.nrows = 6;
    WT1.ncols = 6;
    zero(&WT1);

    /******************** Start the solution *****************************/

    /* set Xp, Yp, and CFL from cam_info */
    Xp = cam_info->Xp;
    Yp = cam_info->Yp;
    CFL = cam_info->CFL;

#ifdef DEBUG
    fprintf(debug, "CAMERA INFO:\n");
    fprintf(debug, "\txp = %f  \typ = %f \tCFL  = %f \n", Xp, Yp, CFL);
#endif

    /* use initial estimates for XC,YC,ZC,omega,phi,kappa
     * and initial standard deviations if provided by i.ortho.photo
     *
     * otherwise set from mean value of all active control points
     * - init_info is generated by photo.init (file INIT_EXP)
     * - init_info->status allows for disactivation of defined values
     */
    if ((init_info != NULL) && (init_info->status == 1)) {
        G_verbose_message("Using provided values for XC,YC,ZC,omega,phi,kappa");
        /* Have initial values */
        *XC = init_info->XC_init;
        *YC = init_info->YC_init;
        *ZC = init_info->ZC_init;
        *Omega = init_info->omega_init;
        *Phi = init_info->phi_init;
        *Kappa = init_info->kappa_init;

        /* weight matrix computed from initial standard variances */
        WT1.x[0][0] = (Q1 / (init_info->XC_var * init_info->XC_var));
        WT1.x[1][1] = (Q1 / (init_info->YC_var * init_info->YC_var));
        WT1.x[2][2] = (Q1 / (init_info->ZC_var * init_info->ZC_var));
        WT1.x[3][3] = (Q1 / (init_info->omega_var * init_info->omega_var));
        WT1.x[4][4] = (Q1 / (init_info->phi_var * init_info->phi_var));
        WT1.x[5][5] = (Q1 / (init_info->kappa_var * init_info->kappa_var));
    }
    else if (active >= 4) { /* set from mean values of active control points */
        double dist_grnd, dist_photo;
        double meanx, meany, meanX, meanY, meanZ;

        G_verbose_message("Estimating values for XC,YC,ZC,omega,phi,kappa");
        /* set initial XC and YC from mean values of control points */
        meanx = meany = meanX = meanY = meanZ = 0;
        x = y = X = Y = 0;
        n = 0;
        first = 1;
        for (i = 0; i < cpz->count; i++) {
            if (cpz->status[i] <= 0)
                continue;

            /* set initial XC, YC */
            if (first) {
                X = cpz->e2[i];
                x = cpz->e1[i];
                Y = cpz->n2[i];
                y = cpz->n1[i];
                first = 0;
            }
            else {
                /* set initial ZC from:                              */
                /* scale ~= dist_photo/dist_grnd  ~= (CFL)/(Z - ZC)  */
                /* ZC ~= Z + CFL(dist_grnd)/(dist_photo)             */
                dx = cpz->e1[i] - x;
                dy = cpz->n1[i] - y;
                dist_photo = sqrt(dx * dx + dy * dy);
                dx = cpz->e2[i] - X;
                dy = cpz->n2[i] - Y;
                dist_grnd = sqrt(dx * dx + dy * dy);

                if (dist_photo != 0 && dist_grnd != 0) {
                    meanZ += cpz->z2[i] + (CFL * (dist_grnd) / (dist_photo));
                    meanx += cpz->e1[i];
                    meany += cpz->n1[i];
                    meanX += cpz->e2[i];
                    meanY += cpz->n2[i];

                    n++;
                }
            }
        }
        if (!n) /* Poorly placed Control Points */
            return -1;

        meanx /= n;
        meany /= n;
        meanX /= n;
        meanY /= n;
        meanZ /= n;
        *XC = meanX;
        *YC = meanY;
        *ZC = meanZ;

        /* set initial rotations to zero (radians) */
        *Omega = *Phi = 0.0;

        /* find an initial kappa */
        kappa1 = atan2((meany - y), (meanx - x));
        kappa2 = atan2((meanY - Y), (meanX - X));
        *Kappa = (kappa2 - kappa1);

        /* set initial weights */
        XC_var = INITIAL_X_VAR;
        YC_var = INITIAL_Y_VAR;
        ZC_var = INITIAL_Z_VAR;
        omega_var = INITIAL_OMEGA_VAR;
        phi_var = INITIAL_PHI_VAR;
        kappa_var = INITIAL_KAPPA_VAR;

        WT1.x[0][0] = (Q1 / (XC_var * XC_var));
        WT1.x[1][1] = (Q1 / (YC_var * YC_var));
        WT1.x[2][2] = (Q1 / (ZC_var * ZC_var));
        WT1.x[3][3] = (Q1 / (omega_var * omega_var));
        WT1.x[4][4] = (Q1 / (phi_var * phi_var));
        WT1.x[5][5] = (Q1 / (kappa_var * kappa_var));
    }
    else {
        G_warning(_("Camera position not available"));
#ifdef DEBUG
        fclose(debug);
#endif
        return (0);
    }

    G_verbose_message("INITIAL CAMERA POSITION:");
    G_verbose_message("XC: %.2f, YC: %.2f, ZC: %.2f", *XC, *YC, *ZC);
    G_verbose_message("Omega %.2f, Phi %.2f, Kappa: %.2f", *Omega * 180. / M_PI,
                      *Phi * 180. / M_PI, *Kappa * 180. / M_PI);

#ifdef DEBUG
    fprintf(debug, "\nINITIAL CAMERA POSITION:\n");
    fprintf(debug, "\tXC = %f  \tYC = %f \tZC = %f \n", *XC, *YC, *ZC);
    fprintf(debug, "\tOMEGA = %f  \tPHI = %f \tKAPPA = %f \n", *Omega, *Phi,
            *Kappa);
#endif

    /* set initial parameters into epsilon matrix */
    epsilon.x[0][0] = *XC;
    epsilon.x[1][0] = *YC;
    epsilon.x[2][0] = *ZC;
    epsilon.x[3][0] = *Omega;
    epsilon.x[4][0] = *Phi;
    epsilon.x[5][0] = *Kappa;

    /* adjust camera position only if enough control points are available */
    max_iters = MAX_ITERS;
    if (active < 4)
        max_iters = -1;

    /************************** Start Iterations *****************************/
    /* itererate until convergence */

    for (iter = 0; iter < max_iters; iter++) {
        /*  break if converged */
        dx = delta.x[0][0];
        dy = delta.x[1][0];
        dz = delta.x[2][0];
        dd = ((dx * dx) + (dy * dy) + (dz * dz));

        if ((iter > 0) && (dd <= CONV_LIMIT))
            break;

#ifdef DEBUG
        fprintf(debug, "\n\tITERATION = %d \n", iter);
#endif

        /* value of parameters at this iteration */
        *XC = epsilon.x[0][0];
        *YC = epsilon.x[1][0];
        *ZC = epsilon.x[2][0];
        *Omega = epsilon.x[3][0];
        *Phi = epsilon.x[4][0];
        *Kappa = epsilon.x[5][0];

#ifdef DEBUG
        fprintf(debug,
                "\n\tepsilon:\n\t\tXC = \t%f \n\t\tYC = \t%f \n\t\tZC = \t%f "
                "\n\t\tomega = \t%f \n\t\tphi = \t%f \n\t\tkappa = \t%f \n\n",
                *XC, *YC, *ZC, *Omega, *Phi, *Kappa);
#endif

        /*  clear NN, CC */
        zero(&NN);
        zero(&CC);

        /*   Set Orientation Matrix from latest vales (Omega, Phi, Kappa); */
        sw = sin(*Omega);
        cw = cos(*Omega);
        sp = sin(*Phi);
        cp = cos(*Phi);
        sk = sin(*Kappa);
        ck = cos(*Kappa);

        /* see  WOLF 1983, Appendix, p. 591 */

        /* ground -> photo */
        M.x[0][0] = (cp * ck);
        M.x[0][1] = (cw * sk) + (sw * sp * ck);
        M.x[0][2] = (sw * sk) - (cw * sp * ck);
        M.x[1][0] = (-cp * sk);
        M.x[1][1] = (cw * ck) - (sw * sp * sk);
        M.x[1][2] = (sw * ck) + (cw * sp * sk);
        M.x[2][0] = sp;
        M.x[2][1] = (-sw * cp);
        M.x[2][2] = (cw * cp);

        /* just an abbreviation of M */
        m11 = M.x[0][0];
        m12 = M.x[0][1];
        m13 = M.x[0][2];
        m21 = M.x[1][0];
        m22 = M.x[1][1];
        m23 = M.x[1][2];
        m31 = M.x[2][0];
        m32 = M.x[2][1];
        m33 = M.x[2][2];

        /* Form Normal equations by summation of all active control points */
        for (i = 0; i < cpz->count; i++) {
#ifdef DEBUG
            fprintf(debug, "\n\t\t\tIn Summation count = %d \n", i);
#endif

            if (cpz->status[i] <= 0)
                continue;

            x = cpz->e1[i];
            y = cpz->n1[i];
            /* z = cpz->z1[i]; */
            X = cpz->e2[i];
            Y = cpz->n2[i];
            Z = cpz->z2[i];

            /* adjust for earth curvature */
            dx = X - *XC;
            dy = Y - *YC;
            dd = (dx * dx) + (dy * dy);
            Z -= dd / (2.0 * ellps_a);

#ifdef DEBUG
            fprintf(debug,
                    "\n\t\t\timage:\n \t\t\tx = \t%f \n\t\t\ty = \t%f "
                    "\n\t\t\tz = \t%f \n\t\t\tobject:\n \t\t\tX = \t%f "
                    "\n\t\t\tY = \t%f \n\t\t\tZ = \t%f \n",
                    x, y, z, X, Y, Z);
#endif

            /* Compute Obj. Space coordinates */
            XYZ.x[0][0] = X - *XC;
            XYZ.x[1][0] = Y - *YC;
            XYZ.x[2][0] = Z - *ZC;

            /* just an abbreviation */
            lam = XYZ.x[0][0];
            mu = XYZ.x[1][0];
            nu = XYZ.x[2][0];

            /* Compute image space coordinates */
            m_mult(&M, &XYZ, &UVW);

            /*  just an abbreviation */
            U = UVW.x[0][0];
            V = UVW.x[1][0];
            W = UVW.x[2][0];

            /* panorama correction
             * in theory either for U, V or for xbar, ybar
             * U, V is recommended because U, V are only used
             * for the residuals
             * correcting U, V also gives a slightly smaller RMSE */
            if (panorama) {
                double a, epan;

                epan = U;
                if (epan < 0) {
                    a = atan2(-epan, -W);
                    epan = -a * -W;
                }
                else {
                    a = atan2(epan, -W);
                    epan = a * -W;
                }
                U = epan;

                V *= cos(a);

                UVW.x[0][0] = U;
                UVW.x[1][0] = V;
            }

            /* Form Partial derivatives of Normal Equations */
            xbar = x - Xp;
            ybar = y - Yp;

            B.x[0][0] = (-Q1 / W) * ((xbar * m31) + (CFL * m11));
            B.x[0][1] = (-Q1 / W) * ((xbar * m32) + (CFL * m12));
            B.x[0][2] = (-Q1 / W) * ((xbar * m33) + (CFL * m13));

            B.x[0][3] = (Q1 / W) * ((xbar * ((-m33 * mu) + (m32 * nu))) +
                                    (CFL * ((-m13 * mu) + (m12 * nu))));
            B.x[0][4] =
                (Q1 / W) *
                ((xbar * ((cp * lam) + (sw * sp * mu) + (-cw * sp * nu))) +
                 (CFL * ((-sp * ck * lam) + (sw * cp * ck * mu) +
                         (-cw * cp * ck * nu))));
            B.x[0][5] =
                (Q1 / W) * (CFL * ((m21 * lam) + (m22 * mu) + (m23 * nu)));

            B.x[1][0] = (-Q1 / W) * ((ybar * m31) + (CFL * m21));
            B.x[1][1] = (-Q1 / W) * ((ybar * m32) + (CFL * m22));
            B.x[1][2] = (-Q1 / W) * ((ybar * m33) + (CFL * m23));

            B.x[1][3] = (Q1 / W) * ((ybar * ((-m33 * mu) + (m32 * nu))) +
                                    (CFL * ((-m23 * mu) + (m22 * nu))));
            B.x[1][4] =
                (Q1 / W) *
                ((ybar * ((cp * lam) + (sw * sp * mu) + (-cw * sp * nu))) +
                 (CFL * ((sp * sk * lam) + (-sw * cp * sk * mu) +
                         (cw * cp * sk * nu))));
            B.x[1][5] =
                (Q1 / W) * (CFL * ((-m11 * lam) + (-m12 * mu) + (-m13 * nu)));

            /* residuals of image coordinates */
            E.x[0][0] = (-Q1) * (xbar + (CFL * (U / W)));
            E.x[1][0] = (-Q1) * (ybar + (CFL * (V / W)));

#ifdef DEBUG
            fprintf(debug,
                    "\n\t\t\tresidual:\n \t\t\tE1 = \t%f \n\t\t\tE2 = \t%f \n",
                    E.x[0][0], E.x[1][0]);
#endif

            /* do the summation into Normal equation and solution matrices */
            /* Find B transpose */
            transpose(&B, &BT);
            /* N = BT*B */
            m_mult(&BT, &B, &N);
            /* NN += N */
            m_add(&NN, &N, &NN);
            /* C = BT*E */
            m_mult(&BT, &E, &C);
            /* CC += C */
            m_add(&CC, &C, &CC);
        } /* end summation loop over all active control points */

#ifdef DEBUG
        fprintf(debug, "\n\tN: \n");
        for (i = 0; i < 6; i++)
            fprintf(debug, "\t%f \t%f \t%f \t%f \t%f \t%f \n", NN.x[i][0],
                    NN.x[i][1], NN.x[i][2], NN.x[i][3], NN.x[i][4], NN.x[i][5]);

        fprintf(debug, "\n\tC: \n");
        fprintf(debug, "\t%f \t%f \t%f \t%f \t%f \t%f \n", CC.x[0][0],
                CC.x[1][0], CC.x[2][0], CC.x[3][0], CC.x[4][0], CC.x[5][0]);
#endif

        /* Add weight matrix of unknowns to NN */
        m_add(&NN, &WT1, &NN);
        /* Solve for delta */

        /* similar but not identical to solvemat in lib/imagery/georef.c */
        if (!inverse(&NN, &N)) { /* control point status becomes zero if matrix
                                    is non-invertable */
            error("Matrix Inversion (Control Points status modified)");
            for (i = 0; i < cpz->count; i++)
                cpz->status[i] = 0;
            return (0);
        }

        /* delta = N-1 * CC */
        m_mult(&N, &CC, &delta);
        /* epsilon += delta */
        m_add(&epsilon, &delta, &epsilon);

#ifdef DEBUG
        fprintf(debug,
                "\ndelta:\n  \n\t\tXC = \t%f \n\t\tYC = \t%f \n\t\tZC = \t%f "
                "\n\t\tomega = \t%f \n\t\tphi = \t%f \n\t\tkappa = \t%f \n",
                delta.x[0][0], delta.x[1][0], delta.x[2][0], delta.x[3][0],
                delta.x[4][0], delta.x[5][0]);
#endif

    } /* end ITERATION loop */
    G_verbose_message("%d iterations to refine camera position", iter);

    /* This is the solution */
    *XC = epsilon.x[0][0];
    *YC = epsilon.x[1][0];
    *ZC = epsilon.x[2][0];
    *Omega = epsilon.x[3][0];
    *Phi = epsilon.x[4][0];
    *Kappa = epsilon.x[5][0];

    G_verbose_message("FINAL CAMERA POSITION:");
    G_verbose_message("XC: %.2f, YC: %.2f, ZC: %.2f", *XC, *YC, *ZC);
    G_verbose_message("Omega %.2f, Phi %.2f, Kappa: %.2f", *Omega * 180. / M_PI,
                      *Phi * 180. / M_PI, *Kappa * 180. / M_PI);

    if (*ZC < 0)
        G_warning(_("Potential BUG in ortholib: camera altitude < 0"));

    /*  Compute Orientation Matrix from Omega, Phi, Kappa */
    /* ground -> photo */
    sw = sin(*Omega);
    cw = cos(*Omega);
    sp = sin(*Phi);
    cp = cos(*Phi);
    sk = sin(*Kappa);
    ck = cos(*Kappa);

    MO->nrows = 3;
    MO->ncols = 3;
    zero(MO);

    MO->x[0][0] = cp * ck;
    MO->x[0][1] = cw * sk + (sw * sp * ck);
    MO->x[0][2] = sw * sk - (cw * sp * ck);
    MO->x[1][0] = -(cp * sk);
    MO->x[1][1] = cw * ck - (sw * sp * sk);
    MO->x[1][2] = sw * ck + (cw * sp * sk);
    MO->x[2][0] = sp;
    MO->x[2][1] = -(sw * cp);
    MO->x[2][2] = cw * cp;

    /* Compute Transposed Orientation Matrix from Omega, Phi, Kappa */
    /* photo -> ground */

    MT->nrows = 3;
    MT->ncols = 3;
    zero(MT);

    /* Transposed Matrix */
    MT->x[0][0] = cp * ck;
    MT->x[1][0] = cw * sk + (sw * sp * ck);
    MT->x[2][0] = sw * sk - (cw * sp * ck);
    MT->x[0][1] = -(cp * sk);
    MT->x[1][1] = cw * ck - (sw * sp * sk);
    MT->x[2][1] = sw * ck + (cw * sp * sk);
    MT->x[0][2] = sp;
    MT->x[1][2] = -(sw * cp);
    MT->x[2][2] = cw * cp;

#ifdef DEBUG
    fclose(debug);
#endif

    return (1);
}

/* given ground coordinates (e1,n1,z1) and the solution from above */
/* compute the photo coordinate (e2,n2) position */
int I_ortho_ref(double e1, double n1, double z1, double *e2, double *n2,
                double *z2 UNUSED, struct Ortho_Camera_File_Ref *cam_info,
                double XC, double YC, double ZC, MATRIX M)
{
    MATRIX UVW, XYZ;
    double U, V, W;
    double /* Xp, Yp, */ CFL;
    double dx, dy, dd;

    /*  Initialize and zero the matrices */
    /*  Object Space Coordinates */
    XYZ.nrows = 3;
    XYZ.ncols = 1;
    zero(&XYZ);
    /*  Image Space coordinates */
    UVW.nrows = 3;
    UVW.ncols = 1;
    zero(&UVW);

    /************************ Start the work ******************************/
    /* Set Xp, Yp, and CFL from cam_info */
    /* Xp = cam_info->Xp; */
    /* Yp = cam_info->Yp; */
    CFL = cam_info->CFL;

    /* adjust for earth curvature */
    dx = e1 - XC;
    dy = n1 - YC;
    dd = (dx * dx) + (dy * dy);
    z1 -= dd / (2.0 * ellps_a);

    /* Object Space (&XYZ, XC,YC,ZC, X,Y,Z); */
    XYZ.x[0][0] = e1 - XC;
    XYZ.x[1][0] = n1 - YC;
    XYZ.x[2][0] = z1 - ZC;

    m_mult(&M, &XYZ, &UVW);

    /* Image Space */
    U = UVW.x[0][0];
    V = UVW.x[1][0];
    W = UVW.x[2][0];

    /* panorama correction could also be done for e2, n2 below
     * results are identical */
    if (panorama) {
        double a, epan;

        epan = U;
        if (epan < 0) {
            a = atan2(-epan, -W);
            epan = -a * -W;
        }
        else {
            a = atan2(epan, -W);
            epan = a * -W;
        }
        U = epan;

        V *= cos(a);
    }

    /* This is the solution */
    *e2 = (-CFL) * (U / W);
    *n2 = (-CFL) * (V / W);

    return (1);
}

/* given the photo coordinates (e1,n1) and elevation z2  */
/* and the solution from I_compute_ortho_equation */
/* compute ground position (e2,n2) */
int I_inverse_ortho_ref(double e1, double n1, double z1, double *e2, double *n2,
                        double *z2 UNUSED,
                        struct Ortho_Camera_File_Ref *cam_info, double XC,
                        double YC, double ZC, MATRIX M)
{
    MATRIX UVW, XYZ;
    double lam, mu, nu;
    double Xp, Yp, CFL;

    /*  Initialize and zero matrices */
    /*  Object Space Coordinates */
    XYZ.nrows = 3;
    XYZ.ncols = 1;
    zero(&XYZ);
    /*  Image Space coordinates */
    UVW.nrows = 3;
    UVW.ncols = 1;
    zero(&UVW);

    /********************** Start the work ********************************/
    /* Set Xp, Yp, and CFL from cam_info */
    Xp = cam_info->Xp;
    Yp = cam_info->Yp;
    CFL = cam_info->CFL;

    /* Image Space */
    UVW.x[0][0] = e1 - Xp;
    UVW.x[1][0] = n1 - Yp;
    UVW.x[2][0] = -CFL;

    if (panorama) {
        double a, epan;

        epan = UVW.x[0][0];
        if (epan < 0) {
            a = -epan / CFL;
            epan = -CFL * tan(a);
        }
        else {
            a = epan / CFL;
            epan = CFL * tan(a);
        }
        UVW.x[0][0] = epan;

        UVW.x[1][0] /= cos(a);
    }

    m_mult(&M, &UVW, &XYZ);

    /* Object Space */
    lam = XYZ.x[0][0];
    mu = XYZ.x[1][0];
    nu = XYZ.x[2][0];

    /* This is the solution */
    *e2 = ((z1 - ZC) * (lam / nu)) + XC;
    *n2 = ((z1 - ZC) * (mu / nu)) + YC;

    return (1);
}

int matrix_error(char *s)
{
    fprintf(stderr, "WARNING: %s", s);
#ifdef DEBUG
    fclose(debug);
#endif
    return 0;
}

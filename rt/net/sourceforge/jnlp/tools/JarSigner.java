/*
 * Copyright 1997-2007 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Sun designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Sun in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 */

package net.sourceforge.jnlp.tools;

import java.io.*;
import java.util.*;
import java.util.zip.*;
import java.util.jar.*;
import java.text.Collator;
import java.text.MessageFormat;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.security.cert.CertPath;
import java.security.*;
import sun.security.x509.*;
import sun.security.util.*;

import net.sourceforge.jnlp.*;
import net.sourceforge.jnlp.cache.*;
import net.sourceforge.jnlp.runtime.*;
import net.sourceforge.jnlp.security.*;

/**
 * <p>The jarsigner utility.
 *
 * @author Roland Schemers
 * @author Jan Luehe
 */

public class JarSigner {

    private static String R(String key) {
        return JNLPRuntime.getMessage(key);
    }

    private static final Collator collator = Collator.getInstance();
    static {
        // this is for case insensitive string comparisions
        collator.setStrength(Collator.PRIMARY);
    }

    private static final String META_INF = "META-INF/";

    // prefix for new signature-related files in META-INF directory
    private static final String SIG_PREFIX = META_INF + "SIG-";


    private static final long SIX_MONTHS = 180*24*60*60*1000L; //milliseconds

    static final String VERSION = "1.0";

    static final int IN_KEYSTORE = 0x01;
    static final int IN_SCOPE = 0x02;

    // signer's certificate chain (when composing)
    X509Certificate[] certChain;

    /*
     * private key
     */
    PrivateKey privateKey;
    KeyStore store;

    IdentityScope scope;

    String keystore; // key store file
    boolean nullStream = false; // null keystore input stream (NONE)
    boolean token = false; // token-based keystore
    String jarfile;  // jar file to sign
    String alias;    // alias to sign jar with
    char[] storepass; // keystore password
    boolean protectedPath; // protected authentication path
    String storetype; // keystore type
    String providerName; // provider name
    Vector<String> providers = null; // list of providers
    HashMap<String,String> providerArgs = new HashMap<String, String>(); // arguments for provider constructors
    char[] keypass; // private key password
    String sigfile; // name of .SF file
    String sigalg; // name of signature algorithm
    String digestalg = "SHA1"; // name of digest algorithm
    String signedjar; // output filename
    String tsaUrl; // location of the Timestamping Authority
    String tsaAlias; // alias for the Timestamping Authority's certificate
    boolean verify = false; // verify the jar
    boolean verbose = false; // verbose output when signing/verifying
    boolean showcerts = false; // show certs when verifying
    boolean debug = false; // debug
    boolean signManifest = true; // "sign" the whole manifest
    boolean externalSF = true; // leave the .SF out of the PKCS7 block

    private boolean hasExpiredCert = false;
    private boolean hasExpiringCert = false;
    private boolean notYetValidCert = false;

    private boolean badKeyUsage = false;
    private boolean badExtendedKeyUsage = false;
    private boolean badNetscapeCertType = false;

    private boolean alreadyTrustPublisher = false;
    private boolean rootInCacerts = false;
    
    /**
     * The single certPath used in this JarSiging. We're only keeping
     * track of one here, since in practice there's only one signer
     * for a JNLP Application.
     */
    private CertPath certPath = null;
    
    private boolean noSigningIssues = true;

    private boolean anyJarsSigned = false;

    /** all of the jar files that were verified */
    private ArrayList<String> verifiedJars = null;

    /** all of the jar files that were not verified */
    private ArrayList<String> unverifiedJars = null;

    /** the certificates used for jar verification */
    private ArrayList<CertPath> certs = null;

    /** details of this signing */
    private ArrayList<String> details = new ArrayList<String>();

    public boolean getAlreadyTrustPublisher() {
    	return alreadyTrustPublisher;
    }
    
    public boolean getRootInCacerts() {
    	return rootInCacerts;
    }
    
    public CertPath getCertPath() {
    	return certPath;
    }
    
    public boolean hasSigningIssues() {
        return hasExpiredCert || notYetValidCert || badKeyUsage
               || badExtendedKeyUsage || badNetscapeCertType;
    }

    public boolean noSigningIssues() {
        return noSigningIssues;
    }

    public boolean anyJarsSigned() {
        return anyJarsSigned;
    }

    public ArrayList<String> getDetails() {
        return details;
    }

    public ArrayList<CertPath> getCerts() {
        return certs;
    }

    public void verifyJars(List<JARDesc> jars, ResourceTracker tracker)
    throws Exception {

        for (int i = 0; i < jars.size(); i++) {

            JARDesc jar = (JARDesc) jars.get(i);
            verifiedJars = new ArrayList<String>();
            unverifiedJars = new ArrayList<String>();
            certs = new ArrayList<CertPath>();

            try {
                String localFile = tracker.getCacheFile(jar.getLocation()).getAbsolutePath();
                boolean result = verifyJar(localFile);
                checkTrustedCerts();

                if (!result) {
                    //allVerified is true until we encounter a problem
                    //with one or more jars
                    noSigningIssues = false;
                    unverifiedJars.add(localFile);
                } else {
                    verifiedJars.add(localFile);
                }
            } catch (Exception e){
                // We may catch exceptions from using verifyJar()
            	// or from checkTrustedCerts	
                throw e;
            }
        }
    }

    public boolean verifyJar(String jarName) throws Exception {
        boolean anySigned = false;
        boolean hasUnsignedEntry = false;
        JarFile jf = null;

        try {
            jf = new JarFile(jarName, true);
            Vector<JarEntry> entriesVec = new Vector<JarEntry>();
            byte[] buffer = new byte[8192];

            Enumeration<JarEntry> entries = jf.entries();
            while (entries.hasMoreElements()) {
                JarEntry je = entries.nextElement();
                entriesVec.addElement(je);
                InputStream is = null;
                try {
                    is = jf.getInputStream(je);
                    int n;
                    while ((n = is.read(buffer, 0, buffer.length)) != -1) {
                        // we just read. this will throw a SecurityException
                        // if  a signature/digest check fails.
                    }
                } finally {
                    if (is != null) {
                        is.close();
                    }
                }
            }

            Manifest man = jf.getManifest();

            if (man != null) {
                if (verbose) System.out.println();
                Enumeration<JarEntry> e = entriesVec.elements();

                long now = System.currentTimeMillis();

                while (e.hasMoreElements()) {
                    JarEntry je = e.nextElement();
                    String name = je.getName();
                    CodeSigner[] signers = je.getCodeSigners();
                    boolean isSigned = (signers != null);
                    anySigned |= isSigned;
                    hasUnsignedEntry |= !je.isDirectory() && !isSigned
                                        && !signatureRelated(name);
                    if (isSigned) {
                    	// TODO: Perhaps we should check here that
                    	// signers.length is only of size 1, and throw an
                    	// exception if it's not?
                        for (int i = 0; i < signers.length; i++) {
                            CertPath certPath = signers[i].getSignerCertPath();
                            if (!certs.contains(certPath))
                                certs.add(certPath);
                            
                            //we really only want the first certPath
                            if (!certPath.equals(this.certPath)){
                            	this.certPath = certPath;
                            }
                            
                            Certificate cert = signers[i].getSignerCertPath()
                                .getCertificates().get(0);
                            if (cert instanceof X509Certificate) {
                                checkCertUsage((X509Certificate)cert, null);
                                if (!showcerts) {
                                    long notAfter = ((X509Certificate)cert)
                                                    .getNotAfter().getTime();

                                    if (notAfter < now) {
                                        hasExpiredCert = true;
                                    } else if (notAfter < now + SIX_MONTHS) {
                                        hasExpiringCert = true;
                                    }
                                }
                            }
                        }
                    }
                } //while e has more elements
            } //if man not null

            //Alert the user if any of the following are true.
            if (!anySigned) {

            } else {
                anyJarsSigned = true;

                //warnings
                if (hasUnsignedEntry || hasExpiredCert || hasExpiringCert ||
                        badKeyUsage || badExtendedKeyUsage || badNetscapeCertType ||
                        notYetValidCert) {

                    addToDetails(R("SRunWithoutRestrictions"));

                    if (badKeyUsage)
                        addToDetails(R("SBadKeyUsage"));
                    if (badExtendedKeyUsage)
                        addToDetails(R("SBadExtendedKeyUsage"));
                    if (badNetscapeCertType)
                        addToDetails(R("SBadNetscapeCertType"));
                    if (hasUnsignedEntry)
                        addToDetails(R("SHasUnsignedEntry"));
                    if (hasExpiredCert)
                        addToDetails(R("SHasExpiredCert"));
                    if (hasExpiringCert)
                        addToDetails(R("SHasExpiringCert"));
                    if (notYetValidCert)
                        addToDetails(R("SNotYetValidCert"));
                }
            }

        } catch (Exception e) {
            e.printStackTrace();
            throw e;
        } finally { // close the resource
            if (jf != null) {
                jf.close();
            }
        }

        //anySigned does not guarantee that all files were signed.
        return anySigned && !(hasUnsignedEntry || hasExpiredCert
                              || badKeyUsage || badExtendedKeyUsage || badNetscapeCertType
                              || notYetValidCert);
    }

    /**
     * Checks the user's trusted.certs file and the cacerts file to see
     * if a publisher's and/or CA's certificate exists there.
     */
    private void checkTrustedCerts() throws Exception {
    	if (certPath != null) {
    		try {
    			KeyTool kt = new KeyTool();
    			alreadyTrustPublisher = kt.isTrusted(getPublisher());
   				rootInCacerts = kt.checkCacertsForCertificate(getRoot());
    		} catch (Exception e) {
    			// TODO: Warn user about not being able to
    			// look through their cacerts/trusted.certs
    			// file depending on exception.
    			throw e;
    		}
    		
    		if (!rootInCacerts)
    			addToDetails(R("SUntrustedCertificate"));
    		else 
    			addToDetails(R("STrustedCertificate"));
    	}
    }
    
    /** 
     * Returns the application's publisher's certificate.
     */
    public Certificate getPublisher() {
    	if (certPath != null) {
    		List<? extends Certificate> certList 
			= certPath.getCertificates();
    		if (certList.size() > 0) {
    			return (Certificate)certList.get(0);
    		} else {
    			return null;
    		}
    	} else {
    		return null;
    	}
    }
    
    /**
     * Returns the application's root's certificate. This
     * may return the same certificate as getPublisher() in
     * the event that the application is self signed.
     */
    public Certificate getRoot() {
    	if (certPath != null) {
    		List<? extends Certificate> certList 
			= certPath.getCertificates();
    		if (certList.size() > 0) {
    			return (Certificate)certList.get(
    				certList.size() - 1);
    		} else {
    			return null;
    		}
    	} else {
    		return null;
    	}
    }
    
	private void addToDetails(String detail) {
		if (!details.contains(detail))
			details.add(detail);
	}

    Hashtable<Certificate, String> storeHash =
        new Hashtable<Certificate, String>();

    /**
     * signature-related files include:
     * . META-INF/MANIFEST.MF
     * . META-INF/SIG-*
     * . META-INF/*.SF
     * . META-INF/*.DSA
     * . META-INF/*.RSA
     *
     * Required for verifyJar()
     */
    private boolean signatureRelated(String name) {
        String ucName = name.toUpperCase();
        if (ucName.equals(JarFile.MANIFEST_NAME) ||
                ucName.equals(META_INF) ||
                (ucName.startsWith(SIG_PREFIX) &&
                 ucName.indexOf("/") == ucName.lastIndexOf("/"))) {
            return true;
        }

        if (ucName.startsWith(META_INF) &&
                SignatureFileVerifier.isBlockOrSF(ucName)) {
            // .SF/.DSA/.RSA files in META-INF subdirs
            // are not considered signature-related
            return (ucName.indexOf("/") == ucName.lastIndexOf("/"));
        }

        return false;
    }

    /**
     * Check if userCert is designed to be a code signer
     * @param userCert the certificate to be examined
     * @param bad 3 booleans to show if the KeyUsage, ExtendedKeyUsage,
     *            NetscapeCertType has codeSigning flag turned on.
     *            If null, the class field badKeyUsage, badExtendedKeyUsage,
     *            badNetscapeCertType will be set.
     *
     * Required for verifyJar()
     */
    void checkCertUsage(X509Certificate userCert, boolean[] bad) {

        // Can act as a signer?
        // 1. if KeyUsage, then [0] should be true
        // 2. if ExtendedKeyUsage, then should contains ANY or CODE_SIGNING
        // 3. if NetscapeCertType, then should contains OBJECT_SIGNING
        // 1,2,3 must be true

        if (bad != null) {
            bad[0] = bad[1] = bad[2] = false;
        }

        boolean[] keyUsage = userCert.getKeyUsage();
        if (keyUsage != null) {
            if (keyUsage.length < 1 || !keyUsage[0]) {
                if (bad != null) {
                    bad[0] = true;
                } else {
                    badKeyUsage = true;
                }
            }
        }

        try {
            List<String> xKeyUsage = userCert.getExtendedKeyUsage();
            if (xKeyUsage != null) {
                if (!xKeyUsage.contains("2.5.29.37.0") // anyExtendedKeyUsage
                        && !xKeyUsage.contains("1.3.6.1.5.5.7.3.3")) {  // codeSigning
                    if (bad != null) {
                        bad[1] = true;
                    } else {
                        badExtendedKeyUsage = true;
                    }
                }
            }
        } catch (java.security.cert.CertificateParsingException e) {
            // shouldn't happen
        }

        try {
            // OID_NETSCAPE_CERT_TYPE
            byte[] netscapeEx = userCert.getExtensionValue
                                ("2.16.840.1.113730.1.1");
            if (netscapeEx != null) {
                DerInputStream in = new DerInputStream(netscapeEx);
                byte[] encoded = in.getOctetString();
                encoded = new DerValue(encoded).getUnalignedBitString()
                .toByteArray();

                NetscapeCertTypeExtension extn =
                    new NetscapeCertTypeExtension(encoded);

                Boolean val = (Boolean)extn.get(
                                  NetscapeCertTypeExtension.OBJECT_SIGNING);
                if (!val) {
                    if (bad != null) {
                        bad[2] = true;
                    } else {
                        badNetscapeCertType = true;
                    }
                }
            }
        } catch (IOException e) {
            //
        }
    }

}





package org.study.lib.md5;

import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/***
 * Provides a static utility interface to an MD5 digest algorithm
 * obtained through the java.security.MessageDigest spi. <p>
 *
 * Database end-users may wish to access the services of this class
 * to provide, for instance, application user lookup tables with
 * one-way password encryption.  For example: <p>
 *
 * <pre>
 * -- DDL
 * CREATE TABLE USERS(UID INTEGER IDENTITY, UNAME VARCHAR, UPASS VARCHAR, UNIQUE(UNAME))
 * CREATE FUNCTION MD5(VARCHAR(100)) RETURNS VARCHAR(16) LANGUAGE JAVA EXTERNAL NAME "org.hsqldb.lib.MD5.encode"
 *
 * -- DML & DQL
 * INSERT INTO USERS(UNAME, UPASS) VALUES('joe', MD5('passwd'))
 * UPDATE USERS SET UPASS = MD5('newpasswd') WHERE UNAME = 'joe' AND UPASS = MD5('oldpasswd')
 * SELECT UID FROM USERS WHERE UNAME = 'joe' AND UPASS = MD5('logonpasswd')
 * </pre>
 *
 * <b>NOTE:</b> <p>
 *
 * Although it is possible that a particular JVM / application installation may
 * encounter NoSuchAlgorithmException when attempting to get a jce MD5 message
 * digest generator, the likelyhood is very small for almost all JDK/JRE 1.1
 * and later  JVM implementations, as the Sun java.security package has come,
 * by default, with a jce MD5 message digest generator since JDK 1.1 was
 * released.  The HSLQLDB project could have provided an MD5 implementation to
 * guarantee presence, but this class is much more lightweight and still allows
 * clients to install / use  custom implementations through the
 * java.security.MessageDigest spi, for instance if there is no service
 * provided by default under the target JVM of choice or if a client has
 * developed / provides, say, a faster MD5 message digest implementation.
 * In short, this class is a convenience that allows HSQLDB SQL Function and
 * Stored Procedure style access to any underlying MD5 message digest algorithm
 * obtained via the java.security.MessageDigest spi
 *
 * @author boucherb@users.sourceforge.net
 * @version 1.9.0
 * @since 1.9.0
 */
public class MD5 {
 
    /***
     * The jce MD5 message digest generator.
     */
    private static MessageDigest md5;
 
    /***
     * Retrieves a hexidecimal character sequence representing the MD5
     * digest of the specified character sequence, using the specified
     * encoding to first convert the character sequence into a byte sequence.
     * If the specified encoding is null, then ISO-8859-1 is assumed
     *
     * @param string the string to encode.
     * @param encoding the encoding used to convert the string into the
     *      byte sequence to submit for MD5 digest
     * @return a hexidecimal character sequence representing the MD5
     *      digest of the specified string
     * @throws HsqlUnsupportedOperationException if an MD5 digest
     *      algorithm is not available through the
     *      java.security.MessageDigest spi or the requested
     *      encoding is not available
     */
    
    public static String bytesToHexString(byte[] src){  
        StringBuilder stringBuilder = new StringBuilder("");  
        if (src == null || src.length <= 0) {  
            return null;  
        }  
        for (int i = 0; i < src.length; i++) {  
            int v = src[i] & 0xFF;  
            String hv = Integer.toHexString(v);  
            if (hv.length() < 2) {  
                stringBuilder.append(0);  
            }  
            stringBuilder.append(hv);  
        }  
        return stringBuilder.toString();  
    }  
    
    public static final String encode(String string,
            String encoding) throws RuntimeException {
       byte[] bytes = digest(string, encoding);
       return bytesToHexString(bytes);
    }
 
    /***
     * Retrieves a byte sequence representing the MD5 digest of the
     * specified character sequence, using the specified encoding to
     * first convert the character sequence into a byte sequence.
     * If the specified encoding is null, then ISO-8859-1 is
     * assumed.
     *
     * @param string the string to digest.
     * @param encoding the character encoding.
     * @return the digest as an array of 16 bytes.
     * @throws HsqlUnsupportedOperationException if an MD5 digest
     *      algorithm is not available through the
     *      java.security.MessageDigest spi or the requested
     *      encoding is not available
     */
    public static byte[] digest(String string,
                                      String encoding)
                                      throws RuntimeException {
 
        byte[] data;
 
        if (encoding == null) {
            encoding = "ISO-8859-1";
        }
 
        try {
            data = string.getBytes(encoding);
        } catch (UnsupportedEncodingException x) {
            throw new RuntimeException(x.toString());
        }
 
        return digest(data);
    }
 
    /***
     * Retrieves a byte sequence representing the MD5 digest of the
     * specified byte sequence.
     *
     * @param data the data to digest.
     * @return the MD5 digest as an array of 16 bytes.
     * @throws HsqlUnsupportedOperationException if an MD5 digest
     *       algorithm is not available through the
     *       java.security.MessageDigest spi
     */
    public static final byte[] digest(byte[] data)
    throws RuntimeException {
 
        synchronized (MD5.class) {
            if (md5 == null) {
                try {
                    md5 = MessageDigest.getInstance("MD5");
                } catch (NoSuchAlgorithmException e) {
                    throw new RuntimeException(e.toString());
                }
            }
 
            return md5.digest(data);
        }
    }
}

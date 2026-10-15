import com.android.apksig.ApkSigner;
import com.android.apksig.ApkVerifier;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.security.cert.X509Certificate;
import java.util.*;

// Passwords and private keys are read locally; never pass them on the command line.
class SignApk {
    public static void main(String[] args) throws Exception {
        char[] password=Files.readString(Path.of(args[1])).strip().toCharArray();
        KeyStore store=KeyStore.getInstance("PKCS12");
        try(var input=new FileInputStream(args[0])) { store.load(input,password); }
        var entry=(KeyStore.PrivateKeyEntry)store.getEntry("ai-builder",new KeyStore.PasswordProtection(password));
        var config=new ApkSigner.SignerConfig.Builder("ai-builder",entry.getPrivateKey(),
            List.of((X509Certificate)entry.getCertificate())).build();
        new ApkSigner.Builder(List.of(config)).setInputApk(new File(args[2])).setOutputApk(new File(args[3]))
            .setMinSdkVersion(26).setV1SigningEnabled(true).setV2SigningEnabled(true).setV3SigningEnabled(true).build().sign();
        var result=new ApkVerifier.Builder(new File(args[3])).build().verify();
        if(!result.isVerified()) throw new IllegalStateException("APK signature verification failed: "+result.getErrors());
        System.out.println("APK signature verified (v2/v3). Certificate SHA-256: "+
            HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(entry.getCertificate().getEncoded())));
        Arrays.fill(password,'\0');
    }
}

#include <stdio.h>
#include <liquid/liquid.h>

int main()
{
    unsigned int n = 1024;  // original data length (bytes)
    fec_scheme fs = LIQUID_FEC_HAMMING74;   // error-correcting scheme
    unsigned int k = fec_get_enc_msg_length(fs,n);  // Compute sizepf encoded message
    // Create arrays
    unsigned char msg_org[n];
    unsigned char msg_enc[k];
    unsigned char msg_dec[n];

    // Create fec object
    fec q = fec_create(fs,NULL);
    fec_print(q);

    unsigned int i;
    // Generate message
    for (i = 0; i < n; i++)
    {
        msg_org[i] = i & 0xff;
    }

    // Encode meesage
    fec_encode(q, n, msg_org, msg_enc);
    
    // Corrupt encoded message
    for (int i=0; i<n; i+=3)
        msg_enc[i] ^= 0x01;

    // Decode message 
    fec_decode(q, n, msg_enc, msg_dec);

    // Destroy the fec object
    fec_destroy(q);

    printf("original message:   [%3u] ", n);
    for (i = 0; i < n; i++)
    {
        printf(" %.2X", msg_org[i]);
    }
    printf("\n");

    printf("decoded message:   [%3u] ", n);
    for (i = 0; i < n; i++)
    {
        printf(" %.2X", msg_dec[i]);
    }
    printf("\n");

    // Count bit errors
    unsigned int num_bit_errors = count_bit_errors_array(msg_org, msg_dec, n);
    printf("number of bits errors received:    %3u / %3u\n", num_bit_errors, n*8);

    printf("done.\n");

    printf("k = %d\n",k);
    return 0;
}
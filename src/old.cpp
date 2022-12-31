            if(0)
            { // Fill the whole buffer with NUM_PERIODS of the waveform
                for(int i=0; i < (NUM_PERIODS*NUM_SAMPLES); i++)
                { // Fill the whole buffer with NUM_PERIODS of the waveform
                    int sample;                             // 16-bit signed little-endian
                    float f;                                // sample as a float [-0.5:0.5]
                    int A;                                  // Amplitude : max = (1<<15) - 1

                    // NOTE: Why do I need my speaker 8x louder than my headphones?

                    if(0)
                    { // headphones
                        A = (1<<11) - 1;
                    }
                    if(1)
                    { // speaker
                        A = (1<<14) - 1;
                    }

                    if(0)
                    { // Sawtooth
                      // f = [0:1]
                      // Say NUM_SAMPLES=400, then f = [0/399:399/399]
                        f = (static_cast<float>(i%NUM_SAMPLES))/(NUM_SAMPLES-1);
                        // f = [-0.5:0.5]
                        f -= 0.5;
                        sample = static_cast<int>(A*f);     // Scale up to amplitude A
                    }
                    if(1)
                    { // Triangle
                      // f = [0:1:0]
                        int TOP = NUM_SAMPLES/2;
                        int n = (i%TOP);
                        if(((i/TOP)%2)!=0) n = (TOP-1)-n;
                        f = (static_cast<float>(n))/(TOP-1);
                        // f = [-0.5:0.5]
                        f -= 0.5;
                        sample = static_cast<int>(A*f);     // Scale up to amplitude A
                    }
                    if(0)
                    { // Noise
                        f = ((static_cast<float>(rand())/RAND_MAX));
                        f -= 0.5;
                        sample = static_cast<int>(A*f);
                    }
                    // Little Endian (LSB at lower address)
                    *buf++ = (Uint8)(sample&0xFF);      // LSB
                    *buf++ = (Uint8)(sample>>8);        // MSB
                }
            }


package org.botdefender.generator;/*
 * The MIT License (MIT)
 * 
 * Copyright (c)2014 Peter Hanlon
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.URL;

public class TrafficGeneratorThread extends Thread {
    static long count=0;

    private synchronized void inc() {
        System.out.println(count++);
    }

    public void run() {
     //   for (int x=0;x<1000000; x++)
        while(true) {
            try {
                URL url = new URL("http://localhost/");
                BufferedReader br = new BufferedReader(new InputStreamReader(url.openStream()));
                String strTemp = "";
                while (null != (strTemp = br.readLine())) {
                    //System.out.println(strTemp);
                }
                br.close();
                inc();
            } catch (Exception ex) {
                ex.printStackTrace();
            }
        }
    }
}

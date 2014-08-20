package org.botdefender.analyzer;

import org.apache.log4j.Logger;

/**
 * Created with IntelliJ IDEA.
 * User: pete.hanlon
 * Date: 20/08/14
 * Time: 11:55
 * To change this template use File | Settings | File Templates.
 */
public class BotDefenderAnalyzer {
    private static Logger logger = Logger.getLogger(BotDefenderAnalyzer.class);

    private void run() {

    }


    public static void main(String args[]) {
        try {
            BotDefenderAnalyzer app = new BotDefenderAnalyzer();
            app.run();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}

import Head from 'next/head';
import styles from '../styles/Home.module.css';

export default function Home() {
  return (
    <div className={styles.container}>
      <Head>
        <title>Smart Parking System</title>
        <meta name="description" content="Smart Parking System API and Dashboard" />
        <link rel="icon" href="/favicon.ico" />
      </Head>

      <main className={styles.main}>
        <h1 className={styles.title}>
          Welcome to Smart Parking System
        </h1>

        <p className={styles.description}>
          API status: Active
        </p>
      </main>

      <footer className={styles.footer}>
        <p>Smart Parking System - ESP32 Firebase Integration</p>
      </footer>
    </div>
  );
}
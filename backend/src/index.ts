import express from 'express';
import cors from 'cors';
import multer from 'multer';
import { spawn, ChildProcessWithoutNullStreams } from 'child_process';
import readline from 'readline';
import path from 'path';
import fs from 'fs';

const app = express();
const PORT = process.env.PORT || 5000;

app.use(cors());
app.use(express.json());

// Set up file storage directory for uploaded documents
const uploadDir = path.join(__dirname, '..', 'uploads');
if (!fs.existsSync(uploadDir)) {
  fs.mkdirSync(uploadDir, { recursive: true });
}

// Multer middleware configuration (storing uploads inside /uploads directory)
const storage = multer.diskStorage({
  destination: (req, file, cb) => {
    cb(null, uploadDir);
  },
  filename: (req, file, cb) => {
    // Generate clean filenames to avoid space conflicts
    const safeName = file.originalname.replace(/[^a-zA-Z0-9.\-_]/g, '_');
    cb(null, `${Date.now()}_${safeName}`);
  }
});
const upload = multer({ storage });

/**
 * High-Performance C++ Subprocess IPC Interface.
 * 
 * Manages the C++ search_core binary daemon:
 * - Automatically restarts C++ core on crash.
 * - Handles JSON serialization of inputs/outputs over stdin/stdout.
 * - Employs a FIFO Queue to align asynchronous Express threads with C++ sequential replies.
 */
class CppProcessManager {
  private child: ChildProcessWithoutNullStreams | null = null;
  private queue: ((data: any) => void)[] = [];
  private rl: readline.Interface | null = null;

  constructor() {
    this.start();
  }

  private start() {
    const isWindows = process.platform === 'win32';
    const binaryName = isWindows ? 'search_core.exe' : 'search_core';
    
    // Look for binary in the parent directory core folder
    const binaryPath = path.join(__dirname, '..', '..', 'core', binaryName);

    console.log(`Spawning high-performance C++ Search Core from: ${binaryPath}`);
    this.child = spawn(binaryPath);

    this.child.on('error', (err) => {
      console.error('\n[WARNING] C++ search_core binary not found or failed to execute.');
      console.error('Please compile the C++ source files first (refer to README.md).\n');
    });

    if (this.child.stderr) {
      this.child.stderr.on('data', (data) => {
        console.error(`C++ Core Log: ${data.toString().trim()}`);
      });
    }

    this.child.on('close', (code) => {
      console.log(`C++ Core process exited with code ${code}.`);
      this.child = null;
      this.queue = []; // Flush active queries
      // Attempt restart in 3 seconds
      setTimeout(() => this.start(), 3000);
    });

    if (this.child.stdout) {
      this.rl = readline.createInterface({
        input: this.child.stdout,
        terminal: false
      });

      this.rl.on('line', (line) => {
        try {
          const json = JSON.parse(line);
          const resolve = this.queue.shift();
          if (resolve) {
            resolve(json);
          }
        } catch (err) {
          console.error('Failed to parse stdout line as JSON:', line, err);
        }
      });
    }
  }

  public sendCommand(cmd: any): Promise<any> {
    return new Promise((resolve, reject) => {
      if (!this.child) {
        return reject(new Error('C++ Search Core process is offline. Make sure it is compiled.'));
      }
      
      this.queue.push(resolve);
      // Write JSON line-separated message to C++ stdin
      this.child.stdin.write(JSON.stringify(cmd) + '\n');
    });
  }
}

const cppManager = new CppProcessManager();

// Track document metadata locally
interface DocumentMeta {
  id: number;
  title: string;
  path: string;
  sizeBytes: number;
}
let indexedDocs: DocumentMeta[] = [];
let nextDocId = 1;

/**
 * Endpoint: POST /api/upload
 * 
 * Receives multi-part file uploads, reads their contents, 
 * pushes them into C++ core index, and saves metadata.
 */
app.post('/api/upload', upload.array('files'), async (req, res) => {
  const files = req.files as Express.Multer.File[];
  if (!files || files.length === 0) {
    return res.status(400).json({ error: 'No files were uploaded.' });
  }

  const results: any[] = [];
  try {
    for (const file of files) {
      const filePath = file.path;
      // Read content as UTF-8 string
      const content = fs.readFileSync(filePath, 'utf-8');
      
      const docId = nextDocId++;
      const docMeta: DocumentMeta = {
        id: docId,
        title: file.originalname,
        path: filePath,
        sizeBytes: file.size
      };

      // Index inside the C++ Core via JSON IPC command
      const response = await cppManager.sendCommand({
        command: 'add_doc',
        id: docId,
        title: docMeta.title,
        path: docMeta.path,
        content: content
      });

      if (response && response.success) {
        indexedDocs.push(docMeta);
        results.push(docMeta);
      } else {
        throw new Error(`C++ failed to index document: ${file.originalname}`);
      }
    }

    res.json({
      message: `Successfully uploaded and indexed ${files.length} document(s).`,
      indexed: results
    });
  } catch (err: any) {
    console.error('Upload failed:', err);
    res.status(500).json({ error: err.message || 'File indexing failed.' });
  }
});

/**
 * Endpoint: GET /api/search
 * 
 * Forwards queries to the C++ core and returns ranked results.
 */
app.get('/api/search', async (req, res) => {
  const { query, type = 'keyword' } = req.query;

  if (!query) {
    return res.status(400).json({ error: 'Search query is required.' });
  }

  try {
    const response = await cppManager.sendCommand({
      command: 'search',
      query: String(query),
      type: String(type)
    });
    res.json(response);
  } catch (err: any) {
    res.status(500).json({ error: err.message || 'Search execution failed.' });
  }
});

/**
 * Endpoint: GET /api/autocomplete
 * 
 * Returns autocompletion suggestions matching the prefix.
 */
app.get('/api/autocomplete', async (req, res) => {
  const { prefix, limit = 5 } = req.query;

  if (!prefix) {
    return res.json({ suggestions: [] });
  }

  try {
    const response = await cppManager.sendCommand({
      command: 'autocomplete',
      prefix: String(prefix),
      limit: Number(limit)
    });
    res.json(response);
  } catch (err: any) {
    res.status(500).json({ error: err.message || 'Autocomplete failed.' });
  }
});

/**
 * Endpoint: GET /api/documents
 * 
 * Returns metadata list of all indexed files.
 */
app.get('/api/documents', (req, res) => {
  res.json({ documents: indexedDocs });
});

/**
 * Endpoint: POST /api/clear
 * 
 * Clears the C++ inverted index memory and deletes uploaded files from disk.
 */
app.post('/api/clear', async (req, res) => {
  try {
    await cppManager.sendCommand({ command: 'clear' });
    
    // Empty the uploads directory on disk
    const files = fs.readdirSync(uploadDir);
    for (const file of files) {
      fs.unlinkSync(path.join(uploadDir, file));
    }
    
    indexedDocs = [];
    nextDocId = 1;
    
    res.json({ success: true, message: 'Inverted Index cleared and local folder cleaned.' });
  } catch (err: any) {
    res.status(500).json({ error: err.message || 'Clear failed.' });
  }
});

app.listen(PORT, () => {
  console.log(`Mini Search Engine API Gateway is running on port ${PORT}`);
});
